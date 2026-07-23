#include <fast_io_core.h>

#include <cassert>
#include <concepts>
#include <string>
#include <type_traits>

namespace
{

struct plan_sink
{
	using output_char_type = char;
	std::string *output;
	std::size_t *scatter_count;
};

// A scatter CPO may be perfectly valid for immediate consumption while still exposing shared scratch that the next
// dynamic plan slot overwrites. The compiled plan retains every descriptor until one final scatter write, so this
// shape-only producer must be rejected by both direct and bound admission.
struct unretained_plan_value
{
	char value{};
};

// This source advertises the required retention marker, but its CPO deliberately throws. Compiled plans must preserve
// that exception instead of terminating through an over-broad noexcept specification.
struct throwing_scatter_value
{
	char value{};
};

// Binding retains this source without invoking status forwarding; the later emission must still propagate the CPO's
// exception after the source lifetime has become stable.
struct throwing_transport_value
{
	char value{};
};

struct incomplete_bound_source;

struct invalid_plan_sink
{};

struct mutable_only_plan_sink
{
	using output_char_type = char;
};

struct const_plan_sink
{
	using output_char_type = char;
};

// Index holes are part of the public arity but not part of the observable source graph. This customization makes any
// accidental normalization visible at runtime while deliberately providing no printable representation.
struct observed_plan_hole
{
	std::size_t *observations;
};

struct deleted_alias_plan_hole
{};

// Repeating one dynamic index must not repeat its source CPO. Apart from performance, a stateful CPO makes that a
// semantic requirement: every component in the plan must describe the same one-time observation.
struct counted_plan_value
{
	char value{};
	std::size_t *observations{};
};

// The bound wrapper must remain printable when its pre-alias source tuple owns a move-only retained source.
struct move_only_plan_value
{
	char value{};

	constexpr move_only_plan_value() noexcept = default;
	constexpr explicit move_only_plan_value(char ch) noexcept : value(ch) {}
	move_only_plan_value(move_only_plan_value const &) = delete;
	move_only_plan_value &operator=(move_only_plan_value const &) = delete;
	constexpr move_only_plan_value(move_only_plan_value &&) noexcept = default;
	constexpr move_only_plan_value &operator=(move_only_plan_value &&) noexcept = default;
};

// A const tuple which stores a reference still exposes the referenced object's original cv-qualification. This source
// deliberately supplies only a mutable-lvalue scatter CPO, catching any admission/body mismatch that adds const while
// materializing a compiled plan descriptor.
struct mutable_only_plan_value
{
	char value{};
};

// The alias points into the source object's own allocation. A saved bound plan must therefore own an rvalue source
// before creating the alias; retaining only the scatter would leave it dangling after the factory full-expression.
struct self_buffer_alias_source
{
	std::string storage;
};

inline observed_plan_hole &status_io_print_forward(
	::fast_io::io_alias_type_t<char>, observed_plan_hole &value) noexcept
{
	++*value.observations;
	return value;
}

inline void print_alias_define(::fast_io::io_alias_t, deleted_alias_plan_hole &) = delete;

inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, counted_plan_value>, counted_plan_value const &value) noexcept
{
	++*value.observations;
	return {__builtin_addressof(value.value), 1u};
}

inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, move_only_plan_value>, move_only_plan_value const &value) noexcept
{
	return {__builtin_addressof(value.value), 1u};
}

inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, mutable_only_plan_value>, mutable_only_plan_value &value) noexcept
{
	return {__builtin_addressof(value.value), 1u};
}

inline ::fast_io::basic_io_scatter_t<char> print_alias_define(
	::fast_io::io_alias_t, self_buffer_alias_source const &value) noexcept
{
	return {value.storage.data(), value.storage.size()};
}

inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, unretained_plan_value>, unretained_plan_value const &value) noexcept
{
	static char scratch;
	scratch = value.value;
	return {__builtin_addressof(scratch), 1u};
}

inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, throwing_scatter_value>, throwing_scatter_value const &)
{
	throw 31;
}

inline ::fast_io::basic_io_scatter_t<char> print_scatter_define(
	::fast_io::io_reserve_type_t<char, throwing_transport_value>, throwing_transport_value const &value) noexcept
{
	return {__builtin_addressof(value.value), 1u};
}

inline throwing_transport_value status_io_print_forward(
	::fast_io::io_alias_type_t<char>, throwing_transport_value &)
{
	throw 29;
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, throwing_scatter_value>) noexcept
{
	return {};
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, throwing_transport_value>) noexcept
{
	return {};
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, counted_plan_value>) noexcept
{
	return {};
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, move_only_plan_value>) noexcept
{
	return {};
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, mutable_only_plan_value>) noexcept
{
	return {};
}

inline constexpr ::std::true_type print_borrowed_scatter_source(
	::fast_io::io_reserve_type_t<char, self_buffer_alias_source>) noexcept
{
	return {};
}

template <typename plan_type, typename value_type>
concept direct_plan_callable = requires(plan_type plan, plan_sink sink, value_type &value) {
	plan.print(sink, value);
};

template <typename plan_type, typename value_type>
concept bound_plan_callable = requires(plan_type plan, value_type &value) {
	plan(value);
};

template <typename plan_type, typename value_type>
concept bound_plan_rvalue_callable = requires(plan_type plan) {
	plan(value_type{});
};

template <typename plan_type, typename value_type>
concept direct_plan_invalid_sink_callable = requires(plan_type plan, invalid_plan_sink sink, value_type &value) {
	plan.print(sink, value);
};

template <typename plan_type, typename value_type>
concept direct_plan_two_values_callable = requires(plan_type plan, plan_sink sink, value_type &first, value_type &second) {
	plan.print(sink, first, second);
};

template <typename plan_type, typename value_type>
concept bound_plan_two_values_callable = requires(plan_type plan, value_type &first, value_type &second) {
	plan(first, second);
};

template <typename plan_type, typename hole_type, typename value_type>
concept direct_plan_pair_callable = requires(
	plan_type plan, plan_sink sink, hole_type &hole, value_type &value) {
	plan.print(sink, hole, value);
};

template <typename plan_type, typename hole_type, typename value_type>
concept bound_plan_pair_callable = requires(plan_type plan, hole_type &hole, value_type &value) {
	plan(hole, value);
};

template <typename component_type>
concept char_scatter_plan_component = requires(component_type component) {
	::fast_io::make_scatter_plan<char>(component);
};

template <typename plan_type, typename values_type>
concept plan_emit_callable = requires(plan_sink sink, values_type const &values) {
	plan_type::emit(sink, values);
};

template <typename plan_type, typename values_type>
concept plan_emit_volatile_values_callable = requires(
	plan_sink sink, values_type const volatile &values) {
	plan_type::emit(sink, values);
};

template <typename plan_type, typename values_type>
concept plan_emit_borrowed_volatile_output_callable = requires(
	plan_sink volatile &sink, values_type const &values) {
	plan_type::emit_borrowed(sink, values);
};

inline constexpr plan_sink output_stream_ref_define(plan_sink sink) noexcept
{
	return sink;
}

[[maybe_unused]] inline void write_all_overflow_define(
	mutable_only_plan_sink &, char const *, char const *) noexcept
{}

[[maybe_unused]] inline void write_all_overflow_define(
	const_plan_sink const &, char const *, char const *) noexcept
{}

// Compiled-plan output is a named observer expression. Its capability proof
// must preserve const instead of borrowing a mutable terminal operation which
// the descriptor emitter cannot invoke.
static_assert(
	!::fast_io::details::decay::compiled_scatter_plan_final_output_v<
		char, mutable_only_plan_sink const &>);
static_assert(
	::fast_io::details::decay::compiled_scatter_plan_final_output_v<
		char, const_plan_sink const &>);
static_assert(
	!::fast_io::details::decay::compiled_scatter_plan_print_output<
		char, mutable_only_plan_sink const &>());
static_assert(
	::fast_io::details::decay::compiled_scatter_plan_print_output<
		char, const_plan_sink const &>());

inline void scatter_write_all_overflow_define(plan_sink sink,
											  fast_io::basic_io_scatter_t<char> const *scatters,
											  std::size_t count)
{
	*sink.scatter_count = count;
	for (std::size_t i{}; i != count; ++i)
	{
		sink.output->append(scatters[i].base, scatters[i].len);
	}
}

#define FAST_IO_COMPILED_SCATTER_TEST_STATIC_SEVEN                              \
	fast_io::mnp::scatter_literal<"|">, fast_io::mnp::scatter_literal<"|">,     \
		fast_io::mnp::scatter_literal<"|">, fast_io::mnp::scatter_literal<"|">, \
		fast_io::mnp::scatter_literal<"|">, fast_io::mnp::scatter_literal<"|">, \
		fast_io::mnp::scatter_literal<"|">

#define FAST_IO_COMPILED_SCATTER_TEST_BLOCK(index)                                  \
	FAST_IO_COMPILED_SCATTER_TEST_STATIC_SEVEN, fast_io::mnp::scatter_literal<"|">, \
		FAST_IO_COMPILED_SCATTER_TEST_STATIC_SEVEN, fast_io::mnp::scatter_dynamic<index>

#define FAST_IO_COMPILED_SCATTER_DYNAMIC_EIGHT(index)      \
	fast_io::mnp::scatter_dynamic<index>,                    \
		fast_io::mnp::scatter_dynamic<index>,                  \
		fast_io::mnp::scatter_dynamic<index>,                  \
		fast_io::mnp::scatter_dynamic<index>,                  \
		fast_io::mnp::scatter_dynamic<index>,                  \
		fast_io::mnp::scatter_dynamic<index>,                  \
		fast_io::mnp::scatter_dynamic<index>,                  \
		fast_io::mnp::scatter_dynamic<index>

#define FAST_IO_COMPILED_SCATTER_DYNAMIC_SIXTY_FOUR(index) \
	FAST_IO_COMPILED_SCATTER_DYNAMIC_EIGHT(index),           \
		FAST_IO_COMPILED_SCATTER_DYNAMIC_EIGHT(index),         \
		FAST_IO_COMPILED_SCATTER_DYNAMIC_EIGHT(index),         \
		FAST_IO_COMPILED_SCATTER_DYNAMIC_EIGHT(index),         \
		FAST_IO_COMPILED_SCATTER_DYNAMIC_EIGHT(index),         \
		FAST_IO_COMPILED_SCATTER_DYNAMIC_EIGHT(index),         \
		FAST_IO_COMPILED_SCATTER_DYNAMIC_EIGHT(index),         \
		FAST_IO_COMPILED_SCATTER_DYNAMIC_EIGHT(index)

} // namespace

int main()
{
	using char_literal_component =
		::std::remove_cvref_t<decltype(fast_io::mnp::scatter_literal<"x">)>;
	using wchar_literal_component =
		::std::remove_cvref_t<decltype(fast_io::mnp::scatter_literal<L"x">)>;
	using char8_literal_component =
		::std::remove_cvref_t<decltype(fast_io::mnp::scatter_literal<u8"x">)>;
	using dynamic_component =
		::std::remove_cvref_t<decltype(fast_io::mnp::scatter_dynamic<0>)>;
	using sparse_dynamic_component =
		::std::remove_cvref_t<decltype(fast_io::mnp::scatter_dynamic<1000000u>)>;
	using low_dynamic_component =
		::std::remove_cvref_t<decltype(fast_io::mnp::scatter_dynamic<1u>)>;
	using maximum_dynamic_component =
		::std::remove_cvref_t<decltype(fast_io::mnp::scatter_dynamic<SIZE_MAX>)>;
	static_assert(char_scatter_plan_component<char_literal_component>);
	static_assert(char_scatter_plan_component<dynamic_component>);
	static_assert(!char_scatter_plan_component<wchar_literal_component>);
	static_assert(!char_scatter_plan_component<char8_literal_component>);
	constexpr auto sparse_reuse_map{
		::fast_io::details::decay::compiled_scatter_plan_previous_dynamic_components<
			sparse_dynamic_component, low_dynamic_component, char_literal_component,
			sparse_dynamic_component, low_dynamic_component, maximum_dynamic_component,
			maximum_dynamic_component>};
	static_assert(sparse_reuse_map.elements[0] == SIZE_MAX);
	static_assert(sparse_reuse_map.elements[1] == SIZE_MAX);
	static_assert(sparse_reuse_map.elements[2] == SIZE_MAX);
	static_assert(sparse_reuse_map.elements[3] == 0u);
	static_assert(sparse_reuse_map.elements[4] == 1u);
	static_assert(sparse_reuse_map.elements[5] == SIZE_MAX);
	static_assert(sparse_reuse_map.elements[6] == 5u);

	constexpr auto one_dynamic_plan{
		fast_io::make_scatter_plan<char>(fast_io::mnp::scatter_dynamic<0>)};
	static_assert(!direct_plan_callable<decltype(one_dynamic_plan), unretained_plan_value>);
	static_assert(!bound_plan_callable<decltype(one_dynamic_plan), unretained_plan_value>);
	static_assert(!bound_plan_callable<decltype(one_dynamic_plan), incomplete_bound_source>);
	static_assert(!direct_plan_invalid_sink_callable<decltype(one_dynamic_plan), fast_io::basic_io_scatter_t<char>>);
	static_assert(!plan_emit_callable<decltype(one_dynamic_plan),
		::fast_io::containers::tuple<unretained_plan_value>>);
	using one_dynamic_tuple =
		::fast_io::containers::tuple<::fast_io::basic_io_scatter_t<char>>;
	using one_dynamic_array = ::fast_io::basic_io_scatter_t<char>[1u];
	// Runtime access deliberately excludes volatile containers and observers. These negative requires tests prove that
	// admission rejects the exact expressions instead of decaying them and failing only in the selected function body.
	static_assert(!plan_emit_volatile_values_callable<decltype(one_dynamic_plan), one_dynamic_tuple>);
	static_assert(!plan_emit_volatile_values_callable<decltype(one_dynamic_plan), one_dynamic_array>);
	static_assert(!plan_emit_borrowed_volatile_output_callable<decltype(one_dynamic_plan), one_dynamic_tuple>);
	static_assert(direct_plan_callable<decltype(one_dynamic_plan), fast_io::basic_io_scatter_t<char>>);
	static_assert(bound_plan_callable<decltype(one_dynamic_plan), fast_io::basic_io_scatter_t<char>>);
	static_assert(direct_plan_callable<decltype(one_dynamic_plan), mutable_only_plan_value>);
	static_assert(bound_plan_callable<decltype(one_dynamic_plan), mutable_only_plan_value>);
	// An lvalue binding retains a mutable reference. An rvalue binding owns its source and later observes it through
	// const&, so a mutable-only scatter protocol is intentionally unavailable in that distinct lifetime model.
	static_assert(!bound_plan_rvalue_callable<decltype(one_dynamic_plan), mutable_only_plan_value>);
	static_assert(bound_plan_rvalue_callable<decltype(one_dynamic_plan), self_buffer_alias_source>);
	// The runtime domain is exact: scatter_dynamic<0> has one observable argument, so an unused second value is ill-formed.
	static_assert(!direct_plan_two_values_callable<decltype(one_dynamic_plan), fast_io::basic_io_scatter_t<char>>);
	static_assert(!bound_plan_two_values_callable<decltype(one_dynamic_plan), fast_io::basic_io_scatter_t<char>>);
	using forged_unretained_bound = ::fast_io::details::decay::compiled_scatter_plan_bound<
		::std::remove_cvref_t<decltype(one_dynamic_plan)>, unretained_plan_value>;
	// Direct aggregate construction cannot bypass the protocol boundary's retained-source proof.
	static_assert(!::fast_io::printable<char, forged_unretained_bound>);
	using forged_volatile_bound = ::fast_io::details::decay::compiled_scatter_plan_bound<
		::std::remove_cvref_t<decltype(one_dynamic_plan)>, counted_plan_value volatile &>;
	// The public aggregate boundary must retain the tuple element's exact volatile reference category. Removing cvref
	// here would incorrectly prove the ordinary const source CPO and fail only after the emission body instantiated.
	static_assert(!::fast_io::printable<char, forged_volatile_bound>);

	constexpr auto out_of_range_plan{
		fast_io::make_scatter_plan<char>(fast_io::mnp::scatter_dynamic<1>)};
	static_assert(!direct_plan_callable<decltype(out_of_range_plan), fast_io::basic_io_scatter_t<char>>);
	static_assert(!bound_plan_callable<decltype(out_of_range_plan), fast_io::basic_io_scatter_t<char>>);
	static_assert(direct_plan_two_values_callable<decltype(out_of_range_plan), fast_io::basic_io_scatter_t<char>>);
	static_assert(bound_plan_two_values_callable<decltype(out_of_range_plan), fast_io::basic_io_scatter_t<char>>);
	static_assert(direct_plan_pair_callable<decltype(out_of_range_plan), observed_plan_hole,
		fast_io::basic_io_scatter_t<char>>);
	static_assert(bound_plan_pair_callable<decltype(out_of_range_plan), observed_plan_hole,
		fast_io::basic_io_scatter_t<char>>);
	static_assert(direct_plan_pair_callable<decltype(out_of_range_plan), deleted_alias_plan_hole,
		fast_io::basic_io_scatter_t<char>>);
	static_assert(bound_plan_pair_callable<decltype(out_of_range_plan), deleted_alias_plan_hole,
		fast_io::basic_io_scatter_t<char>>);
	static_assert(!plan_emit_callable<decltype(out_of_range_plan),
		::fast_io::containers::tuple<fast_io::basic_io_scatter_t<char>>>);
	static_assert(plan_emit_callable<decltype(out_of_range_plan),
		::fast_io::containers::tuple<fast_io::basic_io_scatter_t<char>, fast_io::basic_io_scatter_t<char>>>);
	using forged_out_of_range_bound = ::fast_io::details::decay::compiled_scatter_plan_bound<
		::std::remove_cvref_t<decltype(out_of_range_plan)>, fast_io::basic_io_scatter_t<char>>;
	static_assert(!::fast_io::printable<char, forged_out_of_range_bound>);

	constexpr auto small_plan{fast_io::make_scatter_plan<char>(
		fast_io::mnp::scatter_literal<"user=">, fast_io::mnp::scatter_dynamic<0>,
		fast_io::mnp::scatter_literal<" path=">, fast_io::mnp::scatter_dynamic<1>,
		fast_io::mnp::scatter_literal<"\n">)};
	static_assert(small_plan.scatter_count == 5u);
	static_assert(small_plan.dynamic_count == 2u);
	static_assert(!small_plan.use_blueprint_copy);

	std::string output;
	std::size_t scatter_count{};
	char const name[]{"liyinan"};
	char const path[]{"/api/models"};
	std::size_t hole_observations{};
	observed_plan_hole hole{&hole_observations};
	output.clear();
	out_of_range_plan.print(plan_sink{&output, &scatter_count}, hole,
		fast_io::basic_io_scatter_t<char>{name, 7u});
	assert(hole_observations == 0u);
	assert(output == "liyinan");

	output.clear();
	auto hole_bound{out_of_range_plan(hole, fast_io::basic_io_scatter_t<char>{path, 11u})};
	assert(hole_observations == 0u);
	fast_io::operations::print_freestanding<false>(plan_sink{&output, &scatter_count}, hole_bound);
	assert(hole_observations == 0u);
	assert(output == "/api/models");

	output.clear();
	small_plan.print(plan_sink{&output, &scatter_count}, fast_io::basic_io_scatter_t<char>{name, 7u},
					 fast_io::basic_io_scatter_t<char>{path, 11u});
	assert(scatter_count == 5u);
	assert(output == "user=liyinan path=/api/models\n");

	throwing_scatter_value throwing_scatter{};
	bool scatter_exception_propagated{};
	try
	{
		one_dynamic_plan.print(plan_sink{&output, &scatter_count}, throwing_scatter);
	}
	catch (int code)
	{
		scatter_exception_propagated = code == 31;
	}
	assert(scatter_exception_propagated);

	throwing_transport_value throwing_transport{};
	static_assert(!noexcept(one_dynamic_plan(throwing_transport)));
	bool transport_exception_propagated{};
	auto throwing_transport_bound{one_dynamic_plan(throwing_transport)};
	try
	{
		// Binding now retains the pre-alias source. Status forwarding is deliberately deferred until the source has a
		// stable lifetime and must still propagate its exception from the eventual synchronous emission.
		fast_io::operations::print_freestanding<false>(
			plan_sink{&output, &scatter_count}, throwing_transport_bound);
	}
	catch (int code)
	{
		transport_exception_propagated = code == 29;
	}
	assert(transport_exception_propagated);

	output.clear();
	mutable_only_plan_value mutable_only{'U'};
	one_dynamic_plan.print(plan_sink{&output, &scatter_count}, mutable_only);
	auto mutable_only_bound{one_dynamic_plan(mutable_only)};
	fast_io::operations::print_freestanding<false>(
		plan_sink{&output, &scatter_count}, mutable_only_bound);
	assert(output == "UU");

	output.clear();
	auto self_buffer_bound{
		one_dynamic_plan(self_buffer_alias_source{std::string(64u, 'S')})};
	// Create allocator and stack traffic in a later full-expression. Under the former alias-only binding this made the
	// stale descriptor readily visible to ASan; the owned pre-alias source remains unaffected.
	std::string allocator_traffic(64u, 'X');
	assert(allocator_traffic.front() == 'X');
	fast_io::operations::print_freestanding<false>(
		plan_sink{&output, &scatter_count}, self_buffer_bound);
	assert(output == std::string(64u, 'S'));

	output.clear();
	auto self_buffer_sso_bound{
		one_dynamic_plan(self_buffer_alias_source{std::string("short")})};
	char stack_traffic[128u]{};
	for (std::size_t index{}; index != sizeof(stack_traffic); ++index)
	{
		stack_traffic[index] = static_cast<char>(index);
	}
	assert(stack_traffic[127u] == static_cast<char>(127u));
	fast_io::operations::print_freestanding<false>(
		plan_sink{&output, &scatter_count}, self_buffer_sso_bound);
	assert(output == "short");

	output.clear();
	fast_io::operations::print_freestanding<false>(
		plan_sink{&output, &scatter_count},
		small_plan(fast_io::basic_io_scatter_t<char>{name, 7u},
				   fast_io::basic_io_scatter_t<char>{path, 11u}));
	assert(scatter_count == 5u);
	assert(output == "user=liyinan path=/api/models\n");

	constexpr auto static_plan{fast_io::make_scatter_plan<char>(
		fast_io::mnp::scatter_literal<"ready">, fast_io::mnp::scatter_literal<"\n">)};
	static_assert(static_plan.dynamic_count == 0u);
	output.clear();
	static_plan.print(plan_sink{&output, &scatter_count});
	assert(scatter_count == 2u);
	assert(output == "ready\n");

	constexpr auto repeated_plan{fast_io::make_scatter_plan<char>(
		fast_io::mnp::scatter_dynamic<0>, fast_io::mnp::scatter_literal<"/">,
		fast_io::mnp::scatter_dynamic<0>, fast_io::mnp::scatter_literal<"/">,
		fast_io::mnp::scatter_dynamic<0>)};
	std::size_t repeated_observations{};
	counted_plan_value repeated_value{'R', &repeated_observations};
	output.clear();
	repeated_plan.print(plan_sink{&output, &scatter_count}, repeated_value);
	assert(repeated_observations == 1u);
	assert(output == "R/R/R");

	repeated_observations = 0u;
	output.clear();
	auto repeated_bound{repeated_plan(counted_plan_value{'B', &repeated_observations})};
	fast_io::operations::print_freestanding<false>(plan_sink{&output, &scatter_count}, repeated_bound);
	assert(repeated_observations == 1u);
	assert(output == "B/B/B");

	constexpr auto repeated_sixty_four_plan{fast_io::make_scatter_plan<char>(
		FAST_IO_COMPILED_SCATTER_DYNAMIC_SIXTY_FOUR(0))};
	static_assert(repeated_sixty_four_plan.scatter_count == 64u);
	repeated_observations = 0u;
	output.clear();
	repeated_sixty_four_plan.print(
		plan_sink{&output, &scatter_count}, counted_plan_value{'D', &repeated_observations});
	assert(repeated_observations == 1u);
	assert(output == std::string(64u, 'D'));

	repeated_observations = 0u;
	output.clear();
	auto repeated_sixty_four_bound{repeated_sixty_four_plan(
		counted_plan_value{'E', &repeated_observations})};
	fast_io::operations::print_freestanding<false>(
		plan_sink{&output, &scatter_count}, repeated_sixty_four_bound);
	assert(repeated_observations == 1u);
	assert(output == std::string(64u, 'E'));

	constexpr auto repeated_blueprint_plan{fast_io::make_scatter_plan<char>(
		FAST_IO_COMPILED_SCATTER_TEST_BLOCK(0), FAST_IO_COMPILED_SCATTER_TEST_BLOCK(0),
		FAST_IO_COMPILED_SCATTER_TEST_BLOCK(0), FAST_IO_COMPILED_SCATTER_TEST_BLOCK(0))};
	static_assert(repeated_blueprint_plan.scatter_count == 64u);
	static_assert(repeated_blueprint_plan.dynamic_count == 4u);
	static_assert(repeated_blueprint_plan.use_blueprint_copy);
	std::string repeated_blueprint_expected;
	for (std::size_t block{}; block != 4u; ++block)
	{
		repeated_blueprint_expected.append(15u, '|');
		repeated_blueprint_expected.push_back('P');
	}
	repeated_observations = 0u;
	output.clear();
	counted_plan_value repeated_blueprint_value{'P', &repeated_observations};
	repeated_blueprint_plan.print(
		plan_sink{&output, &scatter_count}, repeated_blueprint_value);
	assert(repeated_observations == 1u);
	assert(output == repeated_blueprint_expected);

	repeated_observations = 0u;
	output.clear();
	auto repeated_blueprint_bound{repeated_blueprint_plan(
		counted_plan_value{'P', &repeated_observations})};
	fast_io::operations::print_freestanding<false>(
		plan_sink{&output, &scatter_count}, repeated_blueprint_bound);
	assert(repeated_observations == 1u);
	assert(output == repeated_blueprint_expected);

	output.clear();
	auto move_only_bound{one_dynamic_plan(move_only_plan_value{'M'})};
	static_assert(::fast_io::printable<char, decltype(move_only_bound)>);
	fast_io::operations::print_freestanding<false>(plan_sink{&output, &scatter_count}, move_only_bound);
	assert(output == "M");

	constexpr auto large_plan{fast_io::make_scatter_plan<char>(
		FAST_IO_COMPILED_SCATTER_TEST_BLOCK(0), FAST_IO_COMPILED_SCATTER_TEST_BLOCK(1),
		FAST_IO_COMPILED_SCATTER_TEST_BLOCK(2), FAST_IO_COMPILED_SCATTER_TEST_BLOCK(3))};
	static_assert(large_plan.scatter_count == 64u);
	static_assert(large_plan.dynamic_count == 4u);
	// Every compiler target applies the same sparse-plan blueprint policy.
	static_assert(large_plan.use_blueprint_copy);

	fast_io::basic_io_scatter_t<char> dynamic_values[]{
		{"0", 1u}, {"1", 1u}, {"2", 1u}, {"3", 1u}};
	output.clear();
	large_plan.print(plan_sink{&output, &scatter_count}, dynamic_values[0], dynamic_values[1], dynamic_values[2],
					 dynamic_values[3]);
	assert(scatter_count == 64u);
	assert(output == "|||||||||||||||0|||||||||||||||1|||||||||||||||2|||||||||||||||3");
}

#undef FAST_IO_COMPILED_SCATTER_TEST_BLOCK
#undef FAST_IO_COMPILED_SCATTER_TEST_STATIC_SEVEN
#undef FAST_IO_COMPILED_SCATTER_DYNAMIC_SIXTY_FOUR
#undef FAST_IO_COMPILED_SCATTER_DYNAMIC_EIGHT
