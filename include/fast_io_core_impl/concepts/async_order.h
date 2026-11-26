#pragma once
#include <utility>

namespace fast_io::async
{
/** @brief Orders the events advertised by two points. A start dependency orders entry into the two providers. */
struct point_dependency
{
	observable_point before, after;
	friend constexpr bool operator==(point_dependency, point_dependency) noexcept = default;
};

/** @brief Describes the covering constraints of a strict partial order. Redundant edges are harmless. */
template <point_dependency... Edges>
struct point_dependencies
{
	static inline constexpr ::std::size_t count{sizeof...(Edges)};
	static inline constexpr point_dependency edges[sizeof...(Edges) + 1]{Edges..., {}};
};

namespace details
{
/** @brief Unites independent contributions from both files and the operation without introducing extra edges. */
template <point_dependency... A, point_dependency... B>
consteval auto join_dependencies(point_dependencies<A...>, point_dependencies<B...>) noexcept
{
	return point_dependencies<A..., B...>{};
}

/** @brief Relocates a child file's relative addresses when a buffer or transcoder wraps that file. */
template <::std::size_t Offset, point_dependency Edge>
consteval point_dependency nested_dependency() noexcept
{
	auto edge{Edge};
	edge.before.depth += Offset;
	edge.after.depth += Offset;
	return edge;
}
template <::std::size_t Offset, point_dependency... Edges>
consteval auto nested_dependencies(point_dependencies<Edges...>) noexcept
{
	return point_dependencies<nested_dependency<Offset, Edges>()...>{};
}

/** @brief Declares only the adjacent start constraints within one directional provider. */
template <point_side Side, ::std::size_t... I>
consteval auto ordered_file_starts(::std::index_sequence<I...>) noexcept
{
	return point_dependencies<point_dependency{{Side, I}, {Side, I + 1u}}...>{};
}

/** @brief Resolves each file's dependencies in the selected direction before rebasing any nested descriptions. */
template <typename File, point_side Side>
consteval auto file_dependencies() noexcept
{
	auto own = [] {
		if constexpr (requires { typename File::local_description; })
			return file_dependencies<typename File::local_description, Side>();
		else if constexpr (requires { typename File::selector_type; } && has_start_points<File>)
			return ordered_file_starts<Side>(::std::make_index_sequence<File::count - 1u>{});
		else if constexpr (requires { async_file_dependencies_define(io_type<File>, ::std::integral_constant<point_side,Side>{}); })
			return async_file_dependencies_define(io_type<File>, ::std::integral_constant<point_side,Side>{});
		else return point_dependencies<>{};
	}();
	if constexpr (requires { typename File::wrapped_child; })
	{
		auto path{join_dependencies(own, nested_dependencies<File::local_count>(file_dependencies<typename File::wrapped_child,Side>()))};
		// Only the provider knows which first entries its data flow guarantees across this boundary.
		if constexpr (requires { async_file_dependencies_define(io_type<File>, ::std::integral_constant<point_side,Side>{}); })
			return join_dependencies(path, async_file_dependencies_define(io_type<File>, ::std::integral_constant<point_side,Side>{}));
		else return path;
	}
	else return own;
}

/** @brief Finds optional operation-level dependencies by ADL on the entire bound file and algorithm type. */
template <typename Graph>
consteval auto operation_dependencies() noexcept
{
	auto files{join_dependencies(file_dependencies<typename Graph::input,point_side::input>(),
		file_dependencies<typename Graph::output,point_side::output>())};
	auto declared=[] {
		using source = typename decltype([] {
			if constexpr (requires { typename Graph::unselected_graph; }) return io_type<typename Graph::unselected_graph>;
			else return io_type<Graph>;
		}())::type;
		if constexpr (requires { async_point_dependencies_define(io_type<source>); }) return async_point_dependencies_define(io_type<source>);
		else return point_dependencies<>{};
	}();
	if constexpr (requires { Graph::value_dependencies(); })
		return join_dependencies(join_dependencies(files,declared),Graph::value_dependencies());
	else return join_dependencies(files,declared);
}

/** @brief Indexes validated edges by one endpoint, preserving the order in which the provider declared them.
 * @details Entry zero ends a list. Each remaining entry identifies one edge and the opposite vertex.
 * Reversing the construction loop makes traversal follow declaration order even for duplicate edges.
 * The point and edge storage is proportional to the schema. An empty schema needs no traversal.
 */
template <typename Graph, typename Order, bool Incoming>
struct dependency_adjacency
{
	::std::size_t head[Graph::count ? Graph::count : 1u]{};
	::std::size_t next[Order::count ? Order::count : 1u]{}, other[Order::count ? Order::count : 1u]{};

	/** @brief Translates each endpoint once after the binding has proved membership. */
	consteval dependency_adjacency() noexcept
	{
		for (::std::size_t i{Order::count}; i; --i)
		{
			auto e{Order::edges[i - 1u]};
			auto from{Graph::index(Incoming ? e.after : e.before)};
			other[i - 1u] = Graph::index(Incoming ? e.before : e.after);
			next[i - 1u] = head[from];
			head[from] = i;
		}
	}
};

/** @brief Shares one immutable successor index across public precedence queries for the same schema. */
template <typename Graph, typename Order>
inline constexpr dependency_adjacency<Graph, Order, false> successor_index{};

/** @brief Enumerates the immediate publication obligations after unselected starts are removed.
 * @details For each visible event, trace incoming edges across hidden starts to the nearest visible
 * predecessors. A per-destination visited set coalesces diamonds. No edge crosses independent operands
 * unless the original provider graph already contains a path between them. A distinct nonzero mark
 * per destination lets all walks reuse a single visited array. The enclosing consteval binding
 * evaluates both passes, including writes to the caller's local count or result buffer.
 */
template <typename Graph, typename Order, typename Emit>
constexpr void visit_visible_dependencies(Emit emit) noexcept
{
	constexpr dependency_adjacency<Graph,Order,true> incoming{};
	::std::size_t seen[Graph::count]{}, todo[Graph::count]{};
	for (::std::size_t i{}; i != Graph::count; ++i)
	{
		auto after{Graph::point(i)};
		if (Graph::starts_at(after) && !Graph::selected_start(after)) continue;
		auto mark{i + 1u};
		::std::size_t n{1}, next{};
		todo[0] = i;
		seen[i] = mark;
		while (next != n)
		{
			for (auto edge{incoming.head[todo[next++]]}; edge; edge = incoming.next[edge - 1u])
			{
				auto k{incoming.other[edge - 1u]};
				if (seen[k] == mark) continue;
				seen[k] = mark;
				auto before{Order::edges[edge - 1u].before};
				if (Graph::starts_at(before) && !Graph::selected_start(before)) todo[n++] = k;
				else emit(point_dependency{before,after});
			}
		}
	}
}

/** @brief Keeps the original sparse edges for fully selected graphs and projects partial selections at bind time. */
template <typename Graph>
consteval auto publication_dependencies() noexcept
{
	using order = decltype(operation_dependencies<Graph>());
	if constexpr (order::count && Graph::has_start_points && [] {
		if constexpr (Graph::has_start_points)
			for (::std::size_t i{}; i != order::count; ++i)
			{
				auto p{order::edges[i].before};
				if (Graph::starts_at(p) && !Graph::selected_start(p)) return true;
			}
		return false;
	}())
	{
		constexpr auto count = [] {
			::std::size_t n{};
			visit_visible_dependencies<Graph,order>([&](point_dependency) { ++n; });
			return n;
		}();
		struct projected { point_dependency edges[count+1u]{}; ::std::size_t size{}; } result;
		visit_visible_dependencies<Graph,order>([&](point_dependency e) { result.edges[result.size++] = e; });
		return result;
	}
	else
	{
		struct original : order { ::std::size_t size{order::count}; };
		return original{};
	}
}

/** @brief Checks at bind time that every dependency belongs to this invocation and that the graph is acyclic.
 * @details If all edges follow the enumeration of the points, that enumeration is a topological order.
 * A backward edge can be valid, for example when an input converter needs its child to read first.
 * For a mixed order, removing a vertex of indegree zero removes each of its outgoing edges exactly
 * once. A remaining nonempty subgraph has a cycle precisely when every remaining vertex has a predecessor.
 * Duplicate edges increment and decrement the same indegree equally. The general check performs a
 * linear number of endpoint lookups and edge visits. It creates no runtime data and adds no dependencies.
 */
template <typename Graph, typename Order>
consteval bool valid_order() noexcept
{
	if constexpr (Order::count == 0) return true;
	else if constexpr (Graph::count == 0) return false;
	else
	{
		bool forward{true};
		for (::std::size_t i{}; i != Order::count; ++i)
		{
			auto e{Order::edges[i]};
			auto a{Graph::index(e.before)}, b{Graph::index(e.after)};
			if (a >= Graph::count || b >= Graph::count || a == b) return false;
			if (a > b) forward = false;
		}
		if (forward) return true;

		dependency_adjacency<Graph,Order,false> outgoing{};
		::std::size_t indegree[Graph::count]{};
		for (::std::size_t i{}; i != Order::count; ++i)
			++indegree[outgoing.other[i]];
		::std::size_t ready[Graph::count]{}, n{}, next{};
		for (::std::size_t i{}; i != Graph::count; ++i) if (!indegree[i]) ready[n++] = i;
		while (next != n)
		{
			for (auto edge{outgoing.head[ready[next++]]}; edge; edge = outgoing.next[edge - 1u])
			{
				auto b{outgoing.other[edge - 1u]};
				if (!--indegree[b]) ready[n++] = b;
			}
		}
		return n == Graph::count;
	}
}
} // namespace details

/** @brief Accepts exactly schemas whose declared dependencies form a strict partial order over their own points. */
template <typename Graph>
concept valid_observable_order = details::valid_order<Graph,decltype(details::operation_dependencies<Graph>())>();

/** @brief Tests strict transitive precedence. Equal, absent and incomparable points all return false.
 * @details Each reached vertex is queued once. The shared index visits just its outgoing edges.
 * A constant-evaluated query can use this same index without emitting a runtime table.
 */
template <valid_observable_order Graph>
[[nodiscard]] constexpr bool point_precedes(observable_point first, observable_point last) noexcept
{
	using order = decltype(details::operation_dependencies<Graph>());
	if constexpr (order::count == 0) return false;
	else
	{
		auto a{Graph::index(first)}, b{Graph::index(last)};
		if (a == b || a >= Graph::count || b >= Graph::count) return false;
		auto const &outgoing{details::successor_index<Graph,order>};
		bool seen[Graph::count]{};
		::std::size_t todo[Graph::count]{}, n{1}, next{};
		todo[0] = a;
		seen[todo[0]] = true;
		while (next != n)
		{
			for (auto edge{outgoing.head[todo[next++]]}; edge; edge = outgoing.next[edge - 1u])
			{
				auto j{outgoing.other[edge - 1u]};
				if (j == b) return true;
				if (!seen[j]) { seen[j] = true; todo[n++] = j; }
			}
		}
		return false;
	}
}
} // namespace fast_io::async
