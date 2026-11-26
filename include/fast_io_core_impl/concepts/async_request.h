#pragma once

#include <utility>

namespace fast_io::async
{
/** @brief Identifies the algorithm interpreting a file-provided description during overload resolution. */
enum class algorithm : unsigned char { print, scan, transmit, format, convert };

/** @brief Gives each file direction its own ADL-associated type, even if two files have equal depth. */
template <typename File, bool Input, typename Description, bool Kernel = false>
struct file_capabilities : Description
{
	using file_type = File;
	static inline constexpr bool kernel{Kernel};
};

/** @brief Detects an explicit kernel guarantee on a file description. A custom description may omit it. */
template <typename Description>
inline constexpr bool kernel_backed_direction = [] {
	if constexpr (requires { ::std::bool_constant<Description::kernel>{}; }) return Description::kernel;
	else return false;
}();

/** @brief Embeds two disjoint point domains, retaining the number and type of this wrapper's own entries. */
template <typename Local, typename Child>
struct composed_file_points
{
	using local_description = Local;
	using wrapped_child = Child;
	static constexpr ::std::size_t local_count{Local::count};
	static_assert(local_count <= static_cast<::std::size_t>(-1) - Child::count);
	static constexpr ::std::size_t count{local_count + Child::count};
	static constexpr bool has_start_points{details::has_start_points<Local> || details::has_start_points<Child>};
	static constexpr point_kind kind(::std::size_t i) noexcept
	{ return i < local_count ? Local::kind(i) : Child::kind(i - local_count); }
	static constexpr bool starts_at(::std::size_t i) noexcept
	{ return i < local_count ? details::describes_start<Local>(i) : details::describes_start<Child>(i - local_count); }
};

/** @brief Describes a wrapper with one ordinary device receipt. */
template <point_kind Outer, typename Child>
using wrapped_file_points = composed_file_points<file_points<Outer>, Child>;

/** @brief Empty direction of a one-ended operation. */
struct no_file_points : file_points<> { static inline constexpr bool kernel{}; };

/** @brief Describes every observable point address without imposing an order on outstanding requests. */
template <algorithm Op, typename Input, typename Output>
struct operation_points
{
	using input = Input;
	using output = Output;
	static inline constexpr algorithm operation{Op};
	/** @brief Each slot belongs to a direction advertised by an endpoint. */
	static inline constexpr ::std::size_t count{Input::count + Output::count};
	static constexpr bool has_start_points{details::has_start_points<Input> || details::has_start_points<Output>};
	/** @brief Only an entry explicitly advertised by the directional provider can report that work has started. */
	static constexpr bool starts_at(observable_point p) noexcept
	{
		if (p.local) return false;
		if (p.side == point_side::input) return details::describes_start<Input>(p.depth);
		return p.side == point_side::output && details::describes_start<Output>(p.depth);
	}

	/** @brief Looks up a point by path, returning count for an unavailable point. */
	static constexpr ::std::size_t index(observable_point p) noexcept
	{
		if (p.local) return count;
		if (p.side == point_side::input) return p.depth < Input::count ? p.depth : count;
		return p.side == point_side::output && p.depth < Output::count ? Input::count + p.depth : count;
	}
	/** @brief Enumerates the schema for introspection; iteration order conveys no scheduling dependency. */
	static constexpr observable_point point(::std::size_t i) noexcept
	{
		if (i < Input::count) return {point_side::input,i};
		return {point_side::output,i-Input::count};
	}
	/** @brief Describes what an in-range publication releases. */
	static constexpr point_kind kind(observable_point p) noexcept
	{
		if (p.side == point_side::input) return Input::kind(p.depth);
		return Output::kind(p.depth);
	}
};

/**
 * @brief Extends an endpoint schema with independently observable argument positions.
 * @details Only opted-in fields consume a point. The original argument index stays stable when
 * adjacent fields are fused, and positional identity never establishes a scheduling dependency.
 */
template <typename Base, bool... Fields>
struct argument_points : Base
{
	static inline constexpr bool fields[sizeof...(Fields) + 1]{Fields...,false};
	static inline constexpr ::std::size_t arguments{sizeof...(Fields)};
	static inline constexpr ::std::size_t count{Base::count + (::std::size_t{} + ... + static_cast<::std::size_t>(Fields))};
	/** @brief Scan commits follow argument order. A print field's completion imposes no global barrier on prefix output. */
	template <::std::size_t N>
	static consteval auto field_dependencies() noexcept
	{
		if constexpr (!fields[N]) return point_dependencies<>{};
		else
		{
			constexpr auto previous=[] {
				auto j=N;
				while (j && !fields[j-1]) --j;
				return j;
			}();
			if constexpr (Base::operation==algorithm::scan && previous!=0)
				return point_dependencies<point_dependency{argument_point<previous-1>,argument_point<N>} >{};
			else return point_dependencies<>{};
		}
	}
	template <::std::size_t N=0>
	static consteval auto value_dependencies() noexcept
	{
		if constexpr (N==arguments) return point_dependencies<>{};
		else return details::join_dependencies(field_dependencies<N>(),value_dependencies<N+1>());
	}
	static constexpr ::std::size_t index(observable_point p) noexcept
	{
		if (p.local) return count;
		if (p.side != point_side::argument)
		{
			auto i{Base::index(p)};
			return i == Base::count ? count : i;
		}
		if (p.depth >= arguments || !fields[p.depth]) return count;
		auto i{Base::count};
		for (::std::size_t j{};j != p.depth;++j) i += fields[j];
		return i;
	}
	static constexpr observable_point point(::std::size_t i) noexcept
	{
		if (i < Base::count) return Base::point(i);
		i -= Base::count;
		for (::std::size_t j{};j != arguments;++j)
			if (fields[j] && i-- == 0) return {point_side::argument,j};
		return {point_side::argument,arguments};
	}
	static constexpr point_kind kind(observable_point p) noexcept
	{ return p.side == point_side::argument ? point_kind::argument : Base::kind(p); }
};

namespace details
{
/** @brief Stores value-selection types without requiring the hosted tuple implementation. */
template <typename... Types>
struct selection_list
{};

/** @brief Selects one type from the freestanding selection list by its compile-time position. */
template <::std::size_t Index, typename List>
struct selection_list_element;

template <typename First, typename... Rest>
struct selection_list_element<0u, selection_list<First, Rest...>>
{
	using type = First;
};

template <::std::size_t Index, typename First, typename... Rest>
struct selection_list_element<Index, selection_list<First, Rest...>> : selection_list_element<Index - 1u, selection_list<Rest...>>
{};

template <::std::size_t Index, typename List>
using selection_list_element_t = typename selection_list_element<Index, List>::type;

/** @brief Carries per-operand selection types into the operation-specific point graph. */
template <typename InputSelection,typename OutputSelection,typename... ValueSelections>
struct operation_request
{
	using input_selection=InputSelection;
	using output_selection=OutputSelection;
	using value_selections=selection_list<ValueSelections...>;
	static inline constexpr ::std::size_t value_count{sizeof...(ValueSelections)};
};

/** @brief Counts the provider's internal milestones followed by its optional final release. */
template <typename Graph, ::std::size_t Argument>
consteval ::std::size_t value_point_count() noexcept
{
	::std::size_t count{};
	if constexpr (requires { Graph::lengths[Argument]; })
		count = Graph::lengths[Argument];
	return count + (Graph::index(argument_point<Argument>) != Graph::count);
}

/** @brief Checks named selections against their immediate provider, recursing through actual child bindings. */
template <typename Selection, typename Description>
consteval bool valid_selection_domain() noexcept
{
	if constexpr (requires { typename Selection::child_selection; typename Description::local_description; })
		return valid_selection_domain<typename Selection::local_selection, typename Description::local_description>() &&
			valid_selection_domain<typename Selection::child_selection, typename Description::wrapped_child>();
	else if constexpr (requires { typename Description::selector_type; Selection::template accepts_domain<typename Description::selector_type>; })
		return Selection::template accepts_domain<typename Description::selector_type>;
	else if constexpr (requires { Selection::template accepts_domain<void>; })
		return Selection::template accepts_domain<void>;
	else return true;
}

/** @brief Checks a marker in the chosen file direction, preserving each decorated child's local domain. */
template <typename Selection, typename Description>
consteval bool valid_file_selection() noexcept
{
	if constexpr (requires { typename Selection::local_selection; })
		return valid_file_selection<typename Selection::local_selection, typename Description::local_description>() &&
			valid_file_selection<typename Selection::child_selection, typename Description::wrapped_child>();
	else if constexpr (Selection::all) return Description::count != 0;
	else return valid_point_selection<Selection, Description::count>() && valid_selection_domain<Selection, Description>();
}

/** @brief Checks a value selection against the domain declared for its original argument position. */
template <typename Selection, typename Graph, ::std::size_t N>
consteval bool valid_value_selection() noexcept
{
	if (!valid_point_selection<Selection, value_point_count<Graph, N>()>()) return false;
	if constexpr (requires { typename Graph::template value_description<N>; })
		return valid_selection_domain<Selection, typename Graph::template value_description<N>>();
	else return valid_selection_domain<Selection, void>();
}

/** @brief Rejects duplicate, unavailable and foreign-enum selectors once the I/O algorithm supplies the actual direction. */
template <typename Request, typename Graph, ::std::size_t... N>
consteval bool valid_request(::std::index_sequence<N...>) noexcept
{
	return valid_file_selection<typename Request::input_selection, typename Graph::input>() &&
		valid_file_selection<typename Request::output_selection, typename Graph::output>() &&
		(valid_value_selection<selection_list_element_t<N,typename Request::value_selections>, Graph, N>() && ...);
}

/** @brief Selects asynchronous work independently for each operand. An unmarked file uses its synchronous driver. */
template <typename Request, typename Graph>
struct selected_point_binding
{
	using graph_type = Graph;
	using input = typename Request::input_selection;
	using output = typename Request::output_selection;
	static constexpr bool async_input{input::all || input::count != 0};
	static constexpr bool async_output{output::all || output::count != 0};

	/** @brief Converts a value publication to the provider's index, placing the final release after its local steps. */
	template <::std::size_t N>
	static constexpr ::std::size_t value_index(observable_point p) noexcept
	{
		return p.local ? p.local - 1 : value_point_count<Graph,N>() - 1;
	}

	/** @brief Runs synchronous predecessors at issue only when every selected predecessor can stay deferred. */
	template <typename Selection>
	static constexpr bool synchronous_prefix(observable_point p) noexcept
	{
		if constexpr (Selection::all || Selection::count == 0) return false;
		else
		{
			bool precedes_selected{};
			for (::std::size_t i{}; i != Graph::count; ++i)
			{
				auto q{Graph::point(i)};
				if (q.side != p.side || (q.side == point_side::argument && q.depth != p.depth) || !selected(q)) continue;
				if (p == q || point_precedes<Graph>(q,p)) return false;
				precedes_selected |= point_precedes<Graph>(p,q);
			}
			return precedes_selected;
		}
	}

	/** @brief The outer layers of a physical buffer stack can run before a selected inner delegate. */
	template <typename Selection, typename Description, point_side Side>
	static constexpr bool file_prefix(observable_point p) noexcept
	{
		if constexpr (Selection::all || Selection::count == 0) return false;
		else if constexpr (requires { Selection::extent; })
			// A child may need input before its parent can finish. Streaming work establishes
			// its own safe issue boundary without waiting on an unbounded transport operation.
			return false;
		else if constexpr (decltype(file_dependencies<Description,Side>())::count != 0)
			return synchronous_prefix<Selection>(p);
		else
		{
			for (auto selected : Selection::indices)
				if (selected <= p.depth) return false;
			return true;
		}
	}

	/** @brief Applies an issue barrier only to unavoidable predecessors of explicitly selected stages. */
	static constexpr bool before_return(observable_point p) noexcept
	{
		// A start is selected when its work is entered. An unselected predecessor runs synchronously at that position.
		if constexpr (Graph::has_start_points) if (Graph::starts_at(p)) return false;
		if (p.side == point_side::input) return file_prefix<input,typename Graph::input,point_side::input>(p);
		if (p.side == point_side::output) return file_prefix<output,typename Graph::output,point_side::output>(p);
		return [&]<::std::size_t... N>(::std::index_sequence<N...>) {
			return ((p.depth == N && synchronous_prefix<selection_list_element_t<N,typename Request::value_selections>>(p)) || ...);
		}(::std::make_index_sequence<Request::value_count>{});
	}

	/** @brief Selects bounded partial progress for a fully annotated provider or an explicit local publication. */
	template <::std::size_t Field>
	using value_selection = selection_list_element_t<Field,typename Request::value_selections>;

	/** @brief Tests a point when a file or a resumable value chooses its local execution strategy. */
	static constexpr bool selected(observable_point p) noexcept
	{
		if (Graph::index(p) == Graph::count) return false;
		if (p.side == point_side::input) return input::selects(p.depth);
		if (p.side == point_side::output) return output::selects(p.depth);
		return [&]<::std::size_t... N>(::std::index_sequence<N...>) {
			return ((p.depth == N && selection_list_element_t<N,typename Request::value_selections>::selects(value_index<N>(p))) || ...);
		}(::std::make_index_sequence<Request::value_count>{});
	}

	static constexpr bool kernel_only = [] {
		if (!async_output) return false;
		// A marked producer may own deferred work even when it publishes no intermediate result.
		if constexpr (!::std::same_as<typename Request::value_selections,selection_list<>>)
			if ([]<typename... V>(selection_list<V...>) { return ((!::std::same_as<V,no_point_selection>) || ...); }
				(typename Request::value_selections{})) return false;
		for (::std::size_t i{}; i < Graph::count; ++i)
		{
			auto p{Graph::point(i)};
			if (selected(p) && Graph::kind(p) != point_kind::transport) return false;
		}
		return true;
	}();
};
} // namespace details

/** @brief Holds a value snapshot of the actual invocation. Reading it does not run or consume the operation. */
template <typename Graph>
struct observation_status
{
	point_state states[Graph::count ? Graph::count : 1u]{};
	bool terminal{}, error{};
	/** @brief Returns unavailable for a point outside this invocation's file and operation types. */
	[[nodiscard]] constexpr point_state state(observable_point p) const noexcept
	{ auto i{Graph::index(p)}; return i == Graph::count ? point_state::unavailable : states[i]; }
	/** @brief Tests whole-operation termination after every owned request has relinquished its operands. */
	[[nodiscard]] constexpr bool done() const noexcept { return terminal; }
};

namespace details
{
/** @brief A kernel-only binding finishes user-defined value generation on the issuing thread. */
template <typename Binding>
inline constexpr bool kernel_only_output = requires { requires Binding::kernel_only; };

/** @brief Binds the direction and value selections only after the algorithm supplies its graph. */
template <typename Request, typename Graph>
using bound_points = selected_point_binding<Request,Graph>;

/** @brief Keeps the compile-time selection on operations that distinguish selected starts from synchronous steps. */
template <typename Graph, typename Binding>
struct bound_start_graph : Graph
{
	using unselected_graph = Graph;
	static constexpr bool selected_start(observable_point p) noexcept
	{ return Graph::starts_at(p) && Binding::selected(p); }
};

/** @brief Leaves every receipt-only operation using its existing graph type and representation. */
template <typename Graph, typename Binding>
using observed_graph = ::std::conditional_t<Graph::has_start_points, bound_start_graph<Graph, Binding>, Graph>;

template <typename Request, typename Graph>
concept binds_points = valid_request<Request,Graph>(::std::make_index_sequence<Request::value_count>{}) &&
	valid_observable_order<Graph>;

/** @brief Detects any creation-time obligation without adding a runtime test to unrestricted requests. */
template <typename B, typename L>
consteval bool has_barrier() noexcept
{
	for (::std::size_t i{}; i != L::count; ++i) if (B::before_return(L::point(i))) return true;
	return false;
}
} // namespace details
} // namespace fast_io::async
