#pragma once

namespace fast_io
{
/** @brief A borrowed engine uses the same instance and inherits its directional suspension guarantee. */
template <typename Engine, bool Input>
	requires async::resumable_transcode_step<Engine, Input>
inline constexpr ::std::true_type async_transcode_step_define(transcode_reserve_t<transcoder_ref<Engine>>, ::std::bool_constant<Input>) noexcept
{
	return {};
}
}

namespace fast_io::async
{
namespace details
{
/** @brief Describes the progress a normalized observer can provide in one direction.
 * @details An explicit description can authorize the blocking driver. A native request and an
 * always-ready memory observer each supply their own boundary. A synchronous stream reference
 * alone supplies no observable work, but remains usable beside a marked value or peer.
 */
template <bool Input, typename T>
consteval auto describe_file()
{
	using R = ::std::remove_cvref_t<T>;
	if constexpr (Input && requires { async_input_levels_define(io_type<R>); })
	{
		using F = decltype(async_input_levels_define(io_type<R>));
		static_assert(file_point_description<F>);
		return async_input_levels_define(io_type<R>);
	}
	else if constexpr (!Input && requires { async_output_levels_define(io_type<R>); })
	{
		using F = decltype(async_output_levels_define(io_type<R>));
		static_assert(file_point_description<F>);
		return async_output_levels_define(io_type<R>);
	}
	else if constexpr (requires { async_stage_child_define(io_type<R>); })
	{
		using Child = typename decltype(async_stage_child_define(io_type<R>))::type;
		auto nested = [] {
			if constexpr (Input) return describe_file<true,decltype(::fast_io::operations::input_stream_ref(::std::declval<Child &>()))>();
			else return describe_file<false,decltype(::fast_io::operations::output_stream_ref(::std::declval<Child &>()))>();
		};
		using C = decltype(nested());
		// The provider describes only its own steps. Child identity and relative addresses survive wrapping.
		using local = decltype([] {
			if constexpr (requires { async_local_points_define(io_type<R>, ::std::bool_constant<Input>{}); })
				return async_local_points_define(io_type<R>, ::std::bool_constant<Input>{});
			else if constexpr (requires { typename R::owner_type::engine_ref_type; })
			{
				// A synchronous engine can still wrap a marked child. Marking this layer requires its own opt-in.
				if constexpr (resumable_transcode_step<typename R::owner_type::engine_ref_type, Input>)
					return file_points<point_kind::decorator>{};
				else return file_points<>{};
			}
			else if constexpr (requires { async_layer_kind_define(io_type<R>); })
				return file_points<async_layer_kind_define(io_type<R>)>{};
			else return file_points<>{};
		}());
		static_assert(file_point_description<local>);
		return file_capabilities<R,Input,composed_file_points<local,C>,kernel_backed_direction<C>>{};
	}
	else if constexpr (requires { async_local_points_define(io_type<R>, ::std::bool_constant<Input>{}); })
	{
		// An opaque observer may explicitly authorize its ordinary operation on the blocking driver.
		using local = decltype(async_local_points_define(io_type<R>, ::std::bool_constant<Input>{}));
		static_assert(file_point_description<local>);
		return file_capabilities<R,Input,local>{};
	}
	else
	{
		constexpr bool kernel{Input ? requires { { async_kernel_input_define(io_type<R>) } -> ::std::same_as<::std::true_type>; } :
			requires { { async_kernel_output_define(io_type<R>) } -> ::std::same_as<::std::true_type>; }};
		constexpr point_kind k{kernel || (Input ? awaitable_input<R> : awaitable_output<R>) ? point_kind::transport :
			(Input ? immediate_input<R> : immediate_output<R>) ? point_kind::memory : point_kind::opaque};
		if constexpr (k == point_kind::opaque)
			return file_capabilities<R,Input,file_points<>>{};
		else return file_capabilities<R,Input,file_points<k>,kernel>{};
	}
}
} // namespace details

} // namespace fast_io::async
