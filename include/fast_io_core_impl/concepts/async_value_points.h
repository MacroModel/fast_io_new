#pragma once

#include <utility>

namespace fast_io::async::details
{
/** @brief Ordinary point storage follows the six operation-wide status flags. */
inline constexpr ::std::size_t first_point_slot{6u};

/** @brief Defaults an adapted value to one final release and a built-in fused value to zero independent releases. */
template <bool Observed>
struct default_value_points : value_points<0> { static constexpr bool observe_field{Observed}; };

/** @brief Obtains the local milestones of the actual print capture or scan binding without examining a synchronous formatter. */
template <bool Scan,typename C,typename T>
consteval auto describe_value()
{
	auto tag{io_reserve_type<C,::std::remove_cvref_t<T>>};
	if constexpr (Scan && async_scan_consumer<C,T> && requires { async_scan_points_define(tag); })
		return async_scan_points_define(tag);
	else if constexpr (!Scan && async_print_producer<C,T> && requires { async_print_points_define(tag); })
		return async_print_points_define(tag);
	else
		{
			if constexpr (Scan) return default_value_points<async_scan_consumer<C,T>>{};
			else return default_value_points<async_print_producer<C,T>>{};
		}
}

/** @brief Addresses local points by original argument position and declares the boundary at which untaken branches close. */
template <typename Base,typename... Descriptions>
struct extended_value_points : Base
{
	template <::std::size_t N>
	using value_description = selection_list_element_t<N, selection_list<Descriptions...>>;
	static constexpr ::std::size_t lengths[sizeof...(Descriptions) + 1]{Descriptions::count...,0};
	static constexpr ::std::size_t count{Base::count+(::std::size_t{}+...+Descriptions::count)};
	static constexpr bool has_start_points{Base::has_start_points || (details::has_start_points<Descriptions> || ...)};

	/** @brief Preserves the distinction between a value starting its work and a target releasing a borrow. */
	static constexpr bool starts_at(observable_point p) noexcept
	{
		if (p.side != point_side::argument) return Base::starts_at(p);
		constexpr bool starts[]{details::has_start_points<Descriptions>..., false};
		return p.depth < sizeof...(Descriptions) && starts[p.depth] && p.local && p.local <= lengths[p.depth];
	}

	/** @brief Maps a public path to a dense status slot while retaining the original indices of ordinary points. */
	static constexpr ::std::size_t index(observable_point p) noexcept
	{
		if (!p.local)
		{
			auto const n{Base::index(p)};
			return n==Base::count ? count : n;
		}
		if (p.side!=point_side::argument || p.depth>=sizeof...(Descriptions) || p.local>lengths[p.depth]) return count;
		auto n{Base::count+p.local-1};
		for (::std::size_t i{};i!=p.depth;++i) n+=lengths[i];
		return n;
	}

	/** @brief Reconstructs the public path for a valid dense status slot. */
	static constexpr observable_point point(::std::size_t i) noexcept
	{
		if (i<Base::count) return Base::point(i);
		i-=Base::count;
		for (::std::size_t field{};field!=sizeof...(Descriptions);++field)
		{
			if (i<lengths[field]) return {point_side::argument,field,i+1};
			i-=lengths[field];
		}
		return {point_side::argument,sizeof...(Descriptions),0};
	}

	/** @brief Locates a local publication after every ordinary argument and file slot of this invocation. */
	static constexpr ::std::size_t local_slot(observable_point p) noexcept
	{
		constexpr ::std::size_t files{Base::input::count>Base::output::count ? Base::input::count : Base::output::count};
		constexpr ::std::size_t span{sizeof...(Descriptions)>files ? sizeof...(Descriptions) : files};
		static_assert(span<=(static_cast<::std::size_t>(-1)-first_point_slot-count)/5u);
		return first_point_slot+5u*span+(index(p)-Base::count);
	}

	/** @brief Rebinds provider-local dependencies to one occurrence, including completion of all its applicable stages. */
	template <::std::size_t Field,typename D,::std::size_t... I,::std::size_t... E>
	static consteval auto field_order(::std::index_sequence<I...>,::std::index_sequence<E...>) noexcept
	{
		if constexpr (details::has_start_points<D>)
			return []<::std::size_t... J>(::std::index_sequence<J...>) {
				return point_dependencies<point_dependency{argument_subpoint<Field,J>,argument_subpoint<Field,J+1u>}...>{};
			}(::std::make_index_sequence<D::count-1u>{});
		else if constexpr (D::count == 0 && D::edge_count == 0) return point_dependencies<>{};
		else return point_dependencies<point_dependency{argument_subpoint<Field,I>,argument_point<Field>}...,
				point_dependency{argument_subpoint<Field,D::edges[E].before>,argument_subpoint<Field,D::edges[E].after>}...>{};
	}

	/** @brief Appends one field's dependency edges without creating an ordering between independent argument positions. */
	template <typename Order>
	static consteval auto append(Order order) noexcept {return order;}
	template <typename Order,typename First,typename... Rest>
	static consteval auto append(Order order,First first,Rest... rest) noexcept
	{return append(join_dependencies(order,first),rest...);}
	template <::std::size_t... N>
	static consteval auto build_order(::std::index_sequence<N...>) noexcept
	{
		constexpr auto base = [] {
			if constexpr (requires { Base::value_dependencies(); }) return Base::value_dependencies();
			else return point_dependencies<>{};
		}();
		return append(base,field_order<N,Descriptions>(::std::make_index_sequence<Descriptions::count>{},
			::std::make_index_sequence<Descriptions::edge_count>{})...);
	}
	static consteval auto value_dependencies() noexcept {return build_order(::std::index_sequence_for<Descriptions...>{});}
};

/** @brief Erases the extension entirely for files and values with no provider-defined milestones. */
template <typename Base,typename... Descriptions>
struct compose_value_points
{
	using type=::std::conditional_t<((Descriptions::count!=0 || Descriptions::edge_count!=0 || requires { typename Descriptions::selector_type; }) || ...),
		extended_value_points<Base,Descriptions...>,Base>;
};
} // namespace fast_io::async::details
