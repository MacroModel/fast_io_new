#pragma once

#include "async_fields.h"
#include "../../fast_io_dsal/impl/misc/push_macros.h"

namespace fast_io::fmt::details
{
/** @brief Maps inserted literal spans around selected fields without adding observable arguments. */
template <basic_fixed_string Literal, typename Grammar, typename Result, typename Graph, typename Request, typename T, typename... A>
inline auto convert_format(T &&target,A &&...args)
{
	namespace op = ::fast_io::async::details;
	using C = typename ::std::remove_cvref_t<decltype(Literal)>::value_type;
	return op::issue_conversion_with<C,Result,Graph,Request>(static_cast<T &&>(target),[&](auto issue) FAST_IO_HERBCEPTIONS_THROWS {
		if constexpr ((::fast_io::async::annotated_operand<A> || ...))
		{
			auto values{::fast_io::containers::forward_as_tuple(static_cast<A &&>(args)...)};
			return [&]<::std::size_t... N>(::std::index_sequence<N...>) FAST_IO_HERBCEPTIONS_THROWS {
				constexpr ::std::size_t offset{::std::is_void_v<Result>};
				return issue(::std::index_sequence<(offset + N/2u)...>{},format_field<N,async_literals<Literal,Grammar,sizeof...(A)>,true>(values)...);
			}(::std::make_index_sequence<2u*sizeof...(A)+1u>{});
		}
		else
		{
			// Every component is frozen during lowering. Only the target can publish a point.
			return lower_format_program_dispatch<Literal,Grammar>([&]<typename... F>(F &&...fields) FAST_IO_HERBCEPTIONS_THROWS {
				return issue(::std::index_sequence_for<F...>{},static_cast<F &&>(fields)...);
			},args...);
		}
	});
}

} // namespace fast_io::fmt::details

#include "../../fast_io_dsal/impl/misc/pop_macros.h"
