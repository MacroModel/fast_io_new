#pragma once

namespace fast_io::async
{
/** @brief Distinguishes the input and output paths when one object is used in both positions. */
enum class point_side : unsigned char { input, output, argument };

/** @brief Classifies the observable boundary supplied by a file, buffer, decorator, or value. */
enum class point_kind : unsigned char { argument, memory, buffer, decorator, transport, opaque };

/** @brief Identifies one operation-local observation by direction, wrapper depth, and optional value-local index. */
struct observable_point
{
	point_side side{};
	::std::size_t depth{};
	::std::size_t local{};
	friend constexpr bool operator==(observable_point, observable_point) noexcept = default;
};

/** @brief Identifies the terminal observation of the value at its original argument position. */
template <::std::size_t Index>
inline constexpr observable_point argument_point{point_side::argument,Index};

/** @brief Identifies one provider-defined internal observation of the value at its original argument position. */
template <::std::size_t Argument, ::std::size_t Local>
	requires (Local != static_cast<::std::size_t>(-1))
inline constexpr observable_point argument_subpoint{point_side::argument, Argument, Local + 1u};

/** @brief Identifies a point in the input wrapper path, with the outermost layer at depth zero. */
template <::std::size_t Depth>
inline constexpr observable_point input_point{point_side::input, Depth};

/** @brief Identifies a point in the output wrapper path, with the outermost layer at depth zero. */
template <::std::size_t Depth>
inline constexpr observable_point output_point{point_side::output, Depth};

/** @brief Reports whether a point is absent, pending, complete, skipped, failed, unreachable, or submitted. */
enum class point_state : unsigned { unavailable, pending, complete, unused, failed, unreached, submitted, started };

/** @brief A file-provided ordered description of all observable points in one normalized direction. */
template <point_kind... Kinds>
struct file_points
{
	static inline constexpr ::std::size_t count{sizeof...(Kinds)};
	/** @brief Returns the requested point kind, with an opaque sentinel for safe constant indexing. */
	static constexpr point_kind kind(::std::size_t index) noexcept
	{
		constexpr point_kind values[]{Kinds..., point_kind::opaque};
		return values[index];
	}
};

/**
 * @brief Declares the locally ordered entries into asynchronous work supplied by one file, converter or value.
 * @details The enum belongs to this provider and direction. Its underlying values address [0, Count).
 * Entering a point publishes one fact for this invocation. The provider remains responsible for completing
 * its work, reporting errors and releasing borrows. Conditional entries may be skipped. Consecutive entries
 * impose start order but do not impose a frame, suspension, whole-stream barrier or public finish event.
 */
template <typename Enum, ::std::size_t Count, point_kind Kind = point_kind::decorator>
	requires ::std::is_enum_v<Enum>
struct ordered_start_points
{
	using selector_type = Enum;
	static constexpr ::std::size_t count{Count};
	static constexpr bool has_start_points{Count != 0};
	static constexpr bool observe_field{false};
	static constexpr ::std::size_t edge_count{}; // The binder generates the adjacent start edges directly.
	static constexpr point_kind kind(::std::size_t) noexcept { return Kind; }
	static constexpr bool starts_at(::std::size_t index) noexcept { return index < count; }
};

namespace details
{
/** @brief Detects explicit start semantics without reinterpreting an existing resource-release description. */
template <typename Description>
inline constexpr bool has_start_points = requires { requires Description::has_start_points; };

/** @brief Queries a local position in descriptions that can mix start points with ordinary device receipts. */
template <typename Description>
constexpr bool describes_start(::std::size_t index) noexcept
{
	if constexpr (has_start_points<Description>) return Description::starts_at(index);
	else return false;
}
} // namespace details

/** @brief Requires an empty, queryable file point description. */
template <typename T>
concept file_point_description = ::std::is_empty_v<T> && requires {
	typename ::std::integral_constant<::std::size_t, T::count>;
	{ T::kind(::std::size_t{}) } noexcept -> ::std::same_as<point_kind>;
};

/** @brief Recognizes an input projection independently of coroutine and scheduler headers. */
template <typename T>
concept input_device = requires(T &value) { input_stream_ref_define(value); } ||
	requires(T &value) { typename ::std::remove_cvref_t<decltype(io_stream_ref_define(value))>::input_char_type; };

/** @brief Recognizes an output projection independently of coroutine and scheduler headers. */
template <typename T>
concept output_device = requires(T &value) { output_stream_ref_define(value); } ||
	requires(T &value) { typename ::std::remove_cvref_t<decltype(io_stream_ref_define(value))>::output_char_type; };

/** @brief Recognizes either direction of an input/output object. */
template <typename T>
concept io_device = input_device<T> || output_device<T>;

/** @brief Accepts integral or enumeration selectors so APIs can use readable, type-specific point names. */
template <auto Value>
inline constexpr bool valid_point_selector =
	(::std::integral<decltype(Value)> && !::std::same_as<decltype(Value), bool>) || ::std::is_enum_v<decltype(Value)>;

/** @brief Converts a numeric or enumeration selector to its zero-based point index. */
template <auto Value>
	requires valid_point_selector<Value>
consteval ::std::size_t point_selector_index() noexcept
{
	if constexpr (::std::is_enum_v<decltype(Value)>)
	{
		using underlying = ::std::underlying_type_t<decltype(Value)>;
		if constexpr (::std::same_as<underlying,bool>) return static_cast<::std::size_t>(Value);
		else return point_selector_index<static_cast<underlying>(Value)>();
	}
	else
	{
		// Reject negative and unrepresentable indices before narrowing on a smaller target.
		if constexpr (::std::is_signed_v<decltype(Value)> && Value < 0)
			return static_cast<::std::size_t>(-1);
		constexpr auto index{static_cast<::std::size_t>(Value)};
		return static_cast<decltype(Value)>(index) == Value ? index : static_cast<::std::size_t>(-1);
	}
}

/** @brief Selects explicit zero-based observation indices, or every available point when the pack is empty. */
template <auto... Indices>
	requires (valid_point_selector<Indices> && ...)
struct point_selection
{
	static inline constexpr bool all{sizeof...(Indices) == 0};
	static inline constexpr ::std::size_t count{sizeof...(Indices)};
	static inline constexpr ::std::size_t indices[count + 1u]{point_selector_index<Indices>()..., static_cast<::std::size_t>(-1)};

	/** @brief Checks uniqueness and representability once per selector pack, independently of its possible providers. */
	static constexpr bool well_formed{[] {
		for (::std::size_t i{}; i != count; ++i)
		{
			if (indices[i] == static_cast<::std::size_t>(-1)) return false;
			for (::std::size_t j{}; j != i; ++j) if (indices[j] == indices[i]) return false;
		}
		return true;
	}()};

	/** @brief Keeps numeric selectors available while checking each named selector against its provider's enum. */
	template <typename Domain>
	static constexpr bool accepts_domain{((!::std::is_enum_v<decltype(Indices)> || ::std::same_as<decltype(Indices), Domain>) && ...)};

	/** @brief Tests whether an available object-local point is selected by this annotation. */
	static constexpr bool selects(::std::size_t index) noexcept
	{
		if constexpr (all) return true;
		else return ((point_selector_index<Indices>() == index) || ...);
	}
};

/** @brief Combines a decorator's local selections with its independently annotated child.
 * @details Each index is checked against the object on which it was written. Child indices
 * are rebased only for the operation's observation graph. An unmarked layer selects no work.
 */
template <typename Local, ::std::size_t LocalCount, typename Child, ::std::size_t ChildCount>
struct decorated_point_selection
{
	using local_selection = Local;
	using child_selection = Child;
	static constexpr ::std::size_t local_count{LocalCount};
private:
	/** @brief Validates local indices before rebasing can hide an invalid index in a child. */
	static consteval bool check() noexcept
	{
		if constexpr (requires { Child::valid; })
			if (!Child::valid) return false;
		for (::std::size_t i{}; i < Local::count; ++i)
		{
			if (Local::indices[i] >= LocalCount) return false;
			for (::std::size_t j{}; j < i; ++j)
				if (Local::indices[i] == Local::indices[j]) return false;
		}
		if constexpr (requires { Child::indices; })
			for (::std::size_t i{}; i < Child::count; ++i)
			{
				if (Child::indices[i] >= ChildCount) return false;
				for (::std::size_t j{}; j < i; ++j)
					if (Child::indices[i] == Child::indices[j]) return false;
			}
		return true;
	}
public:
	/** @brief Tests one global position against the two disjoint local domains. */
	static constexpr bool selects(::std::size_t index) noexcept
	{
		return index < LocalCount ? Local::selects(index) :
			index - LocalCount < ChildCount && Child::selects(index - LocalCount);
	}
	static constexpr ::std::size_t extent{LocalCount + ChildCount};
	static constexpr ::std::size_t count{[] {
		::std::size_t result{};
		for (::std::size_t i{}; i != extent; ++i) result += selects(i);
		return result;
	}()};
	static constexpr bool all{count == extent && count != 0};
	static constexpr bool valid{check()};
};

/** @brief Represents an unannotated value with no asynchronous observations selected. */
struct no_point_selection
{
	static inline constexpr bool all{};
	static inline constexpr ::std::size_t count{};
	static inline constexpr ::std::size_t indices[]{static_cast<::std::size_t>(-1)};
	static constexpr bool selects(::std::size_t) noexcept { return false; }
};

/** @brief Proves an annotation uses unique, valid in-range point indices for one object's point count. */
template <typename Selection, ::std::size_t Count>
consteval bool valid_point_selection() noexcept
{
	if constexpr (requires { Selection::valid; }) return Selection::valid && Selection::extent == Count;
	else if constexpr (Selection::count == 0) return true;
	else if constexpr (requires { Selection::well_formed; })
	{
		if constexpr (!Selection::well_formed) return false;
		else
		{
			for (::std::size_t i{}; i != Selection::count; ++i) if (Selection::indices[i] >= Count) return false;
			return true;
		}
	}
	else
	{
		for (::std::size_t i{}; i != Selection::count; ++i)
		{
			if (Selection::indices[i] >= Count) return false;
			for (::std::size_t j{}; j != i; ++j) if (Selection::indices[i] == Selection::indices[j]) return false;
		}
		return true;
	}
}

} // namespace fast_io::async
