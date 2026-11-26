#pragma once

namespace fast_io::fmt::details
{
/** @brief Lowers ordinary replacement fields and escaped delimiters to compile-time literal spans around async-only values. */
template <basic_fixed_string Literal,bool Percent,::std::size_t Count>
struct async_literal_fields
{
	using C=typename ::std::remove_cvref_t<decltype(Literal)>::value_type;
	C text[::std::remove_cvref_t<decltype(Literal)>::extent]{};
	::std::size_t cuts[Count+2]{};
	bool valid{};
	consteval async_literal_fields() noexcept
	{
		::std::size_t written{},args{};
		for (::std::size_t i{};i<Literal.size();++i)
		{
			auto ch{Literal[i]};
			if constexpr (Percent)
			{
				if (ch=='%')
				{
					if (++i==Literal.size()) return;
					if (Literal[i]=='%') {text[written++]=ch;continue;}
					if (Literal[i]!='s' || args==Count) return;
					cuts[++args]=written;continue;
				}
			}
			else if (ch=='{' || ch=='}')
			{
				if (++i==Literal.size()) return;
				if (Literal[i]==ch) {text[written++]=ch;continue;}
				if (ch!='{' || Literal[i]!='}' || args==Count) return;
				cuts[++args]=written;continue;
			}
			text[written++]=ch;
		}
		valid=args==Count;cuts[Count+1]=written;
	}
	template <::std::size_t N>
	constexpr auto literal() const noexcept
	{ return ::fast_io::basic_io_scatter_t<C>{text+cuts[N],cuts[N+1]-cuts[N]}; }
};
template <basic_fixed_string Literal,typename Grammar,::std::size_t N>
inline constexpr async_literal_fields<Literal,::std::same_as<Grammar,printf_fmt_t>,N> async_literals{};
/** @brief Preserves source argument identities when lowering inserts literal pieces between fields. */
template <::std::size_t N,auto &Program,bool PreserveExtent = false,typename Tuple>
inline constexpr decltype(auto) format_field(Tuple &values)
{
	if constexpr ((N%2u)!=0)
	{
		return ::fast_io::containers::get<N/2u>(static_cast<Tuple &&>(values));
	}
	else if constexpr (PreserveExtent)
	{
		// Only emitted literal units contribute to a conversion's proof. Braces and field syntax consume none.
		constexpr auto span{Program.template literal<N/2u>()};
		if constexpr (span.len == 0u) return ::fast_io::io_null_t{};
		else return ::fast_io::manipulators::static_scatter_t<typename ::std::remove_cvref_t<decltype(Program)>::C, span.len>{span.base};
	}
	else
	{
		return Program.template literal<N/2u>();
	}
}
} // namespace fast_io::fmt::details
