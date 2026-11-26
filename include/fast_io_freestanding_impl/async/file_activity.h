#pragma once

namespace fast_io::async::details
{
/** @brief Tracks actual delegates on the worker performing one async request. Synchronous entry points never install a receipt. */
struct file_activity
{
	::std::size_t reached[2]{};
	bool tracked[2]{};
	// Control traffic may flow backwards while serving one logical read or write.
	enum class direction : unsigned char { inherit, input, output } logical{direction::inherit};
};
#if defined(FAST_IO_ASYNC_HOSTED_CONFIG) && !defined(__SINGLE_THREAD__)
inline thread_local file_activity *current_file_activity{};
#elif defined(__pdp11__) && defined(__GXX_WEAK__) && !__GXX_WEAK__ && !defined(__ELF__)
// A single linker common symbol represents this freestanding scheduler slot across translation units.
extern file_activity *current_file_activity __asm__("__fast_io_async_activity");
__asm__(".comm __fast_io_async_activity,2");
#else
inline file_activity *current_file_activity{};
#endif
struct file_activity_scope
{
	file_activity *previous{current_file_activity};
	explicit file_activity_scope(file_activity &a) noexcept {current_file_activity=::fast_io::freestanding::addressof(a);}
	~file_activity_scope() {current_file_activity=previous;}
};

/** @brief Attaches an address only to the distinct observer type instantiated by the asynchronous driver. */
template <typename R>
struct measured_output
{
	using output_char_type=typename R::output_char_type;
	R *file;
	file_activity *activity;
	::std::size_t depth;
};
template <typename R>
constexpr measured_output<R> output_stream_ref_define(measured_output<R> f) noexcept {return f;}

template <typename R>
void measured_write(R &ref,typename R::output_char_type const *first,typename R::output_char_type const *last,
	file_activity &activity,::std::size_t depth);

/** @brief Preserves ordered scatter fallback through the actual child's typed write channel. */
template <typename R>
inline void write_all_overflow_define(measured_output<R> f,typename R::output_char_type const *a,typename R::output_char_type const *b)
{if (a!=b) measured_write(*f.file,a,b,*f.activity,f.depth);}

/** @brief Reuses each built-in buffer or transcoder's own state and overflow algorithms with a counted child. */
template <typename R>
inline void measured_write(R &ref,typename R::output_char_type const *first,typename R::output_char_type const *last,
	file_activity &activity,::std::size_t depth)
{
	using C=typename R::output_char_type;
	activity.tracked[1]=true;
	if (activity.reached[1]<depth) activity.reached[1]=depth;
	if constexpr (requires { async_measured_write_define(ref,first,last,activity,depth); })
	{
		async_measured_write_define(ref,first,last,activity,depth);
	}
	else if constexpr (requires { typename R::io_buffer_type; ref.iobptr->output_buffer; ref.iobptr->handle; })
	{
		if (output_stream_try_write_define(ref,first,last)) return;
		decltype(auto) child{::fast_io::operations::output_stream_ref(ref.iobptr->handle)};
		measured_output<::std::remove_reference_t<decltype(child)>> wrapped{::fast_io::freestanding::addressof(child),&activity,depth+1};
		using traits=typename R::io_buffer_type::traits_type;
		::fast_io::details::io_buffer::write_all_overflow_impl<C,typename traits::allocator_type,traits::output_buffer_size>(
			wrapped,ref.iobptr->output_buffer,first,last);
	}
	else if constexpr (requires { ref.ptr->public_buffer; ref.ptr->transcode_engine; ref.ptr->handle.get(); })
	{
		if (output_stream_try_write_define(ref,first,last)) return;
		auto &file{*ref.ptr};
		file.require_typed_write_boundary();
		file.guarded([&] {
			using traits=typename R::owner_type::traits_type;
			auto n{static_cast<::std::size_t>(last-first)};
			if (file.public_buffer.buffer_begin!=file.public_buffer.buffer_curr || n>=traits::public_buffer_size)
			{
				decltype(auto) engine{::fast_io::transcode_ref(file.transcode_engine)};
				decltype(auto) child{::fast_io::operations::output_stream_ref(file.handle.get())};
				measured_output<::std::remove_reference_t<decltype(child)>> wrapped{::fast_io::freestanding::addressof(child),&activity,depth+1};
				file.process_public_prefix(engine,wrapped);
				if (n>=traits::public_buffer_size) {file.process_public_units(engine,wrapped,first,last);return;}
			}
			file.ensure_public_buffer();
			file.public_buffer.buffer_curr=::fast_io::details::non_overlapped_copy(first,last,file.public_buffer.buffer_curr);
		});
	}
	else ::fast_io::operations::decay::write_all_decay_dispatch(ref,first,last);
}

/** @brief Preserves the ordinary inline write when the operation has no nested file to observe. */
template <typename R,typename C>
inline void write_observed(R &ref,C const *first,C const *last)
{
	if constexpr (requires { async_stage_child_define(io_type<::std::remove_cvref_t<R>>); })
	{
		if (auto *a{current_file_activity}) {measured_write(ref,first,last,*a,0);return;}
	}
	::fast_io::operations::decay::write_all_decay_dispatch(ref,first,last);
}
} // namespace fast_io::async::details
