#include <cstddef>
#include <cstdlib>
#include <type_traits>

#include <fast_io.h>

namespace arg_env_decay_ownership
{

inline void require(bool condition) noexcept
{
	if (!condition)
	{
		::std::abort();
	}
}

struct ownership_counts
{
	::std::size_t source_aliases{};
	::std::size_t proxy_aliases{};
	::std::size_t copies{};
	::std::size_t moves{};
	::std::size_t formats{};
	::std::size_t live{};
};

struct counting_source
{
	char value{};
	ownership_counts *counts{};
};

/// A deliberately observable value transport. Eight arguments used to copy every recursive suffix and therefore
/// exceeded any linear construction bound even before the individual formatting calls were reached.
struct counting_proxy
{
	char value{};
	ownership_counts *counts{};
	bool engaged{true};

	inline explicit counting_proxy(char character, ownership_counts *observations) noexcept
		: value(character), counts(observations)
	{
		++counts->live;
	}

	inline counting_proxy(counting_proxy const &other) noexcept
		: value(other.value), counts(other.counts), engaged(other.engaged)
	{
		++counts->copies;
		++counts->live;
	}

	inline counting_proxy(counting_proxy &&other) noexcept
		: value(other.value), counts(other.counts), engaged(other.engaged)
	{
		++counts->moves;
		++counts->live;
		other.engaged = false;
	}

	counting_proxy &operator=(counting_proxy const &) = delete;
	counting_proxy &operator=(counting_proxy &&) = delete;

	inline ~counting_proxy()
	{
		--counts->live;
	}
};

inline counting_proxy print_alias_define(
	::fast_io::io_alias_t, counting_source &source) noexcept
{
	++source.counts->source_aliases;
	return counting_proxy{source.value, source.counts};
}

inline counting_proxy &print_alias_define(
	::fast_io::io_alias_t, counting_proxy &proxy) noexcept
{
	++proxy.counts->proxy_aliases;
	return proxy;
}

struct move_only_source
{
	char value{};
	ownership_counts *counts{};
};

/// This proxy proves that the pack owner may materialize a prvalue exactly once but no internal helper may reconstruct it.
struct move_only_proxy
{
	char value{};
	ownership_counts *counts{};
	bool engaged{true};

	inline explicit move_only_proxy(char character, ownership_counts *observations) noexcept
		: value(character), counts(observations)
	{
		++counts->live;
	}

	move_only_proxy(move_only_proxy const &) = delete;
	move_only_proxy &operator=(move_only_proxy const &) = delete;

	inline move_only_proxy(move_only_proxy &&other) noexcept
		: value(other.value), counts(other.counts), engaged(other.engaged)
	{
		++counts->moves;
		++counts->live;
		other.engaged = false;
	}

	move_only_proxy &operator=(move_only_proxy &&) = delete;

	inline ~move_only_proxy()
	{
		--counts->live;
	}
};

inline move_only_proxy print_alias_define(
	::fast_io::io_alias_t, move_only_source &source) noexcept
{
	++source.counts->source_aliases;
	return move_only_proxy{source.value, source.counts};
}

inline move_only_proxy &print_alias_define(
	::fast_io::io_alias_t, move_only_proxy &proxy) noexcept
{
	++proxy.counts->proxy_aliases;
	return proxy;
}

struct borrowed_observations
{
	::std::size_t source_aliases{};
	::std::size_t proxy_aliases{};
	::std::size_t formats{};
	void const *observed_address{};
};

struct borrowed_proxy
{
	char value{};
	borrowed_observations *observations{};
	bool alive{true};

	inline explicit borrowed_proxy(
		char character, borrowed_observations *state) noexcept
		: value(character), observations(state)
	{}

	borrowed_proxy(borrowed_proxy const &) = delete;
	borrowed_proxy &operator=(borrowed_proxy const &) = delete;
	borrowed_proxy(borrowed_proxy &&) = delete;
	borrowed_proxy &operator=(borrowed_proxy &&) = delete;

	inline ~borrowed_proxy()
	{
		alive = false;
	}
};

struct borrowed_source
{
	borrowed_proxy proxy;

	inline explicit borrowed_source(
		char character, borrowed_observations *observations) noexcept
		: proxy(character, observations)
	{}
};

inline borrowed_proxy &print_alias_define(
	::fast_io::io_alias_t, borrowed_source &source) noexcept
{
	++source.proxy.observations->source_aliases;
	return source.proxy;
}

inline borrowed_proxy &print_alias_define(
	::fast_io::io_alias_t, borrowed_proxy &proxy) noexcept
{
	++proxy.observations->proxy_aliases;
	return proxy;
}

template <::std::integral char_type>
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char_type, counting_proxy>) noexcept
{
	return 1u;
}

template <::std::integral char_type>
inline char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<char_type, counting_proxy>, char_type *destination,
	counting_proxy &proxy) noexcept
{
	require(proxy.engaged);
	++proxy.counts->formats;
	*destination = static_cast<char_type>(proxy.value);
	return destination + 1u;
}

template <::std::integral char_type>
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char_type, move_only_proxy>) noexcept
{
	return 1u;
}

template <::std::integral char_type>
inline char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<char_type, move_only_proxy>, char_type *destination,
	move_only_proxy &proxy) noexcept
{
	require(proxy.engaged);
	++proxy.counts->formats;
	*destination = static_cast<char_type>(proxy.value);
	return destination + 1u;
}

template <::std::integral char_type>
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char_type, borrowed_proxy>) noexcept
{
	return 1u;
}

template <::std::integral char_type>
inline char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<char_type, borrowed_proxy>, char_type *destination,
	borrowed_proxy &proxy) noexcept
{
	require(proxy.alive);
	++proxy.observations->formats;
	proxy.observations->observed_address = __builtin_addressof(proxy);
	*destination = static_cast<char_type>(proxy.value);
	return destination + 1u;
}

static_assert(!::std::is_copy_constructible_v<move_only_proxy>);
static_assert(!::std::is_move_constructible_v<borrowed_proxy>);

template <typename char_type>
inline void require_code_unit(char_type actual, char expected) noexcept
{
	require(actual == static_cast<char_type>(expected));
}

#if (defined(_WIN32) && !defined(__WINE__)) || defined(__CYGWIN__)

template <typename string_type>
inline void require_quoted_sequence(string_type const &value, ::std::size_t count) noexcept
{
	auto const *data{value.data()};
	::std::size_t position{};
	for (::std::size_t index{}; index != count; ++index)
	{
		require_code_unit(data[position++], '"');
		require_code_unit(data[position++], static_cast<char>('A' + index));
		require_code_unit(data[position++], '"');
		require_code_unit(data[position++], ' ');
	}
	require(value.size() == position);
	require_code_unit(data[position], '\0');
}

inline void test_win32_args_linear_ownership()
{
	ownership_counts counts;
	::fast_io::win32_process_args arguments{
		counting_source{'A', __builtin_addressof(counts)},
		counting_source{'B', __builtin_addressof(counts)},
		counting_source{'C', __builtin_addressof(counts)},
		counting_source{'D', __builtin_addressof(counts)},
		counting_source{'E', __builtin_addressof(counts)},
		counting_source{'F', __builtin_addressof(counts)},
		counting_source{'G', __builtin_addressof(counts)},
		counting_source{'H', __builtin_addressof(counts)}};

	require(counts.source_aliases == 8u);
	require(counts.proxy_aliases == 0u);
	require(counts.copies <= 8u);
	require(counts.formats == 8u);
	require(counts.live == 0u);
	require_quoted_sequence(arguments.args, 8u);
}

inline void test_win32_env_linear_ownership()
{
	ownership_counts counts;
	::fast_io::win32_process_envs environment{
		counting_source{'A', __builtin_addressof(counts)},
		counting_source{'B', __builtin_addressof(counts)},
		counting_source{'C', __builtin_addressof(counts)},
		counting_source{'D', __builtin_addressof(counts)},
		counting_source{'E', __builtin_addressof(counts)},
		counting_source{'F', __builtin_addressof(counts)},
		counting_source{'G', __builtin_addressof(counts)},
		counting_source{'H', __builtin_addressof(counts)}};

	require(counts.source_aliases == 8u);
	require(counts.proxy_aliases == 0u);
	require(counts.copies <= 8u);
	require(counts.formats == 8u);
	require(counts.live == 0u);
	require(environment.envs.size() == 16u);
	auto const *data{environment.envs.data()};
	for (::std::size_t index{}; index != 8u; ++index)
	{
		require_code_unit(data[index * 2u], static_cast<char>('A' + index));
		require_code_unit(data[index * 2u + 1u], '\0');
	}
	// The last explicit entry terminator and basic_string's implicit terminator form the required double NUL.
	require_code_unit(data[16u], '\0');
}

inline void test_win32_move_only_and_reference_lifetime()
{
	ownership_counts move_counts;
	::fast_io::win32_process_args moving{
		move_only_source{'M', __builtin_addressof(move_counts)},
		move_only_source{'N', __builtin_addressof(move_counts)}};
	require(move_counts.source_aliases == 2u);
	require(move_counts.proxy_aliases == 0u);
	require(move_counts.copies == 0u);
	require(move_counts.formats == 2u);
	require(move_counts.live == 0u);
	require(moving.args.size() == 8u);
	require_code_unit(moving.args[1u], 'M');
	require_code_unit(moving.args[5u], 'N');

	ownership_counts environment_move_counts;
	::fast_io::win32_process_envs moving_environment{
		move_only_source{'M', __builtin_addressof(environment_move_counts)},
		move_only_source{'N', __builtin_addressof(environment_move_counts)}};
	require(environment_move_counts.source_aliases == 2u);
	require(environment_move_counts.proxy_aliases == 0u);
	require(environment_move_counts.copies == 0u);
	require(environment_move_counts.formats == 2u);
	require(environment_move_counts.live == 0u);
	require(moving_environment.envs.size() == 4u);
	require_code_unit(moving_environment.envs[0u], 'M');
	require_code_unit(moving_environment.envs[1u], '\0');
	require_code_unit(moving_environment.envs[2u], 'N');
	require_code_unit(moving_environment.envs[3u], '\0');
	require_code_unit(moving_environment.envs[4u], '\0');

	borrowed_observations observations;
	borrowed_source source{'R', __builtin_addressof(observations)};
	::fast_io::win32_process_args borrowed{source};
	require(observations.source_aliases == 1u);
	require(observations.proxy_aliases == 0u);
	require(observations.formats == 1u);
	require(observations.observed_address == __builtin_addressof(source.proxy));
	require_code_unit(borrowed.args[1u], 'R');
}

inline void test_win32_codecvt_fallback()
{
	// A UTF-8 static scatter is not directly printable to the native UTF-16 process domain, so this reaches the
	// source-only codecvt continuation without reopening normalization for any already-owned process proxy.
	::fast_io::win32_process_args arguments{u8"CV"};
	require(arguments.args.size() == 5u);
	require_code_unit(arguments.args[0u], '"');
	require_code_unit(arguments.args[1u], 'C');
	require_code_unit(arguments.args[2u], 'V');
	require_code_unit(arguments.args[3u], '"');
	require_code_unit(arguments.args[4u], ' ');
	require_code_unit(arguments.args[5u], '\0');

	::fast_io::win32_process_envs environment{u8"CV"};
	require(environment.envs.size() == 3u);
	require_code_unit(environment.envs[0u], 'C');
	require_code_unit(environment.envs[1u], 'V');
	require_code_unit(environment.envs[2u], '\0');
	require_code_unit(environment.envs[3u], '\0');
}

#else

inline void require_posix_sequence(
	::fast_io::posix_process_args const &arguments, ::std::size_t count) noexcept
{
	auto const *argv{arguments.get_argv()};
	for (::std::size_t index{}; index != count; ++index)
	{
		require(argv[index] != nullptr);
		require(argv[index][0u] == static_cast<char>('A' + index));
		require(argv[index][1u] == '\0');
	}
	require(argv[count] == nullptr);
}

inline void test_posix_linear_ownership()
{
	ownership_counts counts;
	::fast_io::posix_process_args arguments{
		counting_source{'A', __builtin_addressof(counts)},
		counting_source{'B', __builtin_addressof(counts)},
		counting_source{'C', __builtin_addressof(counts)},
		counting_source{'D', __builtin_addressof(counts)},
		counting_source{'E', __builtin_addressof(counts)},
		counting_source{'F', __builtin_addressof(counts)},
		counting_source{'G', __builtin_addressof(counts)},
		counting_source{'H', __builtin_addressof(counts)}};

	require(counts.source_aliases == 8u);
	require(counts.proxy_aliases == 0u);
	require(counts.copies <= 8u);
	require(counts.formats == 8u);
	require(counts.live == 0u);
	require_posix_sequence(arguments, 8u);
}

inline void test_posix_move_only_and_reference_lifetime()
{
	ownership_counts move_counts;
	::fast_io::posix_process_args moving{
		move_only_source{'M', __builtin_addressof(move_counts)},
		move_only_source{'N', __builtin_addressof(move_counts)}};
	require(move_counts.source_aliases == 2u);
	require(move_counts.proxy_aliases == 0u);
	require(move_counts.copies == 0u);
	require(move_counts.formats == 2u);
	require(move_counts.live == 0u);
	auto const *moving_argv{moving.get_argv()};
	require(moving_argv[0u][0u] == 'M' && moving_argv[0u][1u] == '\0');
	require(moving_argv[1u][0u] == 'N' && moving_argv[1u][1u] == '\0');
	require(moving_argv[2u] == nullptr);

	borrowed_observations observations;
	borrowed_source source{'R', __builtin_addressof(observations)};
	::fast_io::posix_process_args borrowed{source};
	require(observations.source_aliases == 1u);
	require(observations.proxy_aliases == 0u);
	require(observations.formats == 1u);
	require(observations.observed_address == __builtin_addressof(source.proxy));
	auto const *borrowed_argv{borrowed.get_argv()};
	require(borrowed_argv[0u][0u] == 'R' && borrowed_argv[0u][1u] == '\0');
	require(borrowed_argv[1u] == nullptr);
}

inline void test_posix_empty_argument_is_not_terminator()
{
	::fast_io::posix_process_args arguments{""};
	auto const *argv{arguments.get_argv()};
	require(argv[0u] != nullptr);
	require(argv[0u][0u] == '\0');
	require(argv[1u] == nullptr);
}

inline void test_posix_codecvt_fallback()
{
	// A UTF-16 static scatter cannot enter the char dispatcher directly. Its codecvt carrier is a new local source whose
	// borrowed input range remains alive until the synchronous unforwarded continuation completes.
	::fast_io::posix_process_args arguments{u"CV"};
	auto const *argv{arguments.get_argv()};
	require(argv[0u] != nullptr);
	require(argv[0u][0u] == 'C');
	require(argv[0u][1u] == 'V');
	require(argv[0u][2u] == '\0');
	require(argv[1u] == nullptr);
}

#endif

} // namespace arg_env_decay_ownership

int main()
{
#if (defined(_WIN32) && !defined(__WINE__)) || defined(__CYGWIN__)
	arg_env_decay_ownership::test_win32_args_linear_ownership();
	arg_env_decay_ownership::test_win32_env_linear_ownership();
	arg_env_decay_ownership::test_win32_move_only_and_reference_lifetime();
	arg_env_decay_ownership::test_win32_codecvt_fallback();
#else
	arg_env_decay_ownership::test_posix_linear_ownership();
	arg_env_decay_ownership::test_posix_move_only_and_reference_lifetime();
	arg_env_decay_ownership::test_posix_empty_argument_is_not_terminator();
	arg_env_decay_ownership::test_posix_codecvt_fallback();
#endif
}
