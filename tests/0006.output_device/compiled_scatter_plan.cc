#include <fast_io_core.h>

#include <cassert>
#include <string>

namespace
{

struct plan_sink
{
	using output_char_type = char;
	std::string *output;
	std::size_t *scatter_count;
};

inline constexpr plan_sink output_stream_ref_define(plan_sink sink) noexcept
{
	return sink;
}

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

} // namespace

int main()
{
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
	small_plan.print(plan_sink{&output, &scatter_count}, fast_io::basic_io_scatter_t<char>{name, 7u},
					 fast_io::basic_io_scatter_t<char>{path, 11u});
	assert(scatter_count == 5u);
	assert(output == "user=liyinan path=/api/models\n");

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
