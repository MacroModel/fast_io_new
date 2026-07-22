#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

namespace fast_io_json_benchmark
{

struct adapter
{
	::std::string_view name{};
	void *state{};
	::std::size_t (*parse)(void *state){};
	::std::size_t (*serialize)(void *state){};
	::std::string (*serialize_text)(void *state){};
	void (*destroy)(void *state) noexcept {};
};

template <typename value_type>
inline void benchmark_barrier(value_type const &value) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
	__asm__ __volatile__("" : : "g"(::std::addressof(value)) : "memory");
#else
	(void)value;
#endif
}

[[noreturn]] void benchmark_failure(char const *message);

[[nodiscard]] adapter make_fast_io_adapter(::std::string_view input);
[[nodiscard]] adapter make_fast_io_std_adapter(::std::string_view input);
[[nodiscard]] adapter make_fast_io_immutable_adapter(::std::string_view input);
[[nodiscard]] adapter make_fast_io_immutable_std_adapter(::std::string_view input);
[[nodiscard]] adapter make_yyjson_immutable_adapter(::std::string_view input);
[[nodiscard]] adapter make_yyjson_mutable_adapter(::std::string_view input);
[[nodiscard]] adapter make_rapidjson_adapter(::std::string_view input);
[[nodiscard]] adapter make_simdjson_adapter(::std::string_view input);
[[nodiscard]] adapter make_glaze_adapter(::std::string_view input);

} // namespace fast_io_json_benchmark
