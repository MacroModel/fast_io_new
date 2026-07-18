#pragma once

namespace fast_io
{

namespace manipulators
{

template <::std::integral chartype, typename T>
struct basic_transcoder_t
{
	using char_type = chartype;
	using transcoder_value_type = T;
	using manip_tag = ::fast_io::manip_tag_t;
	::fast_io::basic_io_scatter_t<char_type> reference;
#ifndef __INTELLISENSE__
#if __has_cpp_attribute(msvc::no_unique_address)
	[[msvc::no_unique_address]]
#else
	[[no_unique_address]]
#endif
#endif
	transcoder_value_type transcoder;
};

/// Creates a non-owning transcode request from two explicitly borrowed lvalues.
///
/// The request stores a character scatter and a pointer to the transcoder.  It
/// cannot extend either lifetime, so accepting a temporary source or transcoder
/// would leave a dangling member before a later print operation starts.  The
/// source-side borrowed marker is deliberately checked before aliasing: the
/// shape of a returned scatter alone cannot prove that its characters outlive
/// this wrapper.
template <typename stryp, typename T>
	requires(::std::is_lvalue_reference_v<stryp &&> && ::std::is_lvalue_reference_v<T &&> &&
			 alias_printable<stryp> &&
			 requires(stryp &&source) {
				 typename ::std::remove_cvref_t<decltype(::fast_io::io_print_alias(
					 ::std::forward<stryp>(source)))>::value_type;
				 requires ::std::same_as<
					 ::std::remove_cvref_t<decltype(::fast_io::io_print_alias(
						 ::std::forward<stryp>(source)))>,
					 ::fast_io::basic_io_scatter_t<typename ::std::remove_cvref_t<decltype(
						 ::fast_io::io_print_alias(::std::forward<stryp>(source)))>::value_type>>;
				 requires ::fast_io::borrowed_scatter_source<
					 typename ::std::remove_cvref_t<decltype(::fast_io::io_print_alias(
						 ::std::forward<stryp>(source)))>::value_type,
					 stryp>;
			 })
inline constexpr auto transcode(stryp &&sct, T &&t)
{
	auto source{::fast_io::io_print_alias(::std::forward<stryp>(sct))};
	using source_type = ::std::remove_cvref_t<decltype(source)>;
	return ::fast_io::manipulators::basic_transcoder_t<
		typename source_type::value_type, ::std::remove_reference_t<T> *>{
		source, __builtin_addressof(t)};
}

template <::std::integral to_char_type, ::std::integral from_char_type, typename T>
inline constexpr auto status_io_print_forward(::fast_io::io_alias_type_t<to_char_type>, ::fast_io::manipulators::basic_transcoder_t<from_char_type, T *> t)
{
	return ::fast_io::manipulators::basic_transcoder_t<from_char_type, decltype(transcode_forward_ref<from_char_type, to_char_type>(*t.transcode))>{t.reference, transcode_forward_ref<from_char_type, to_char_type>(*t.transcode)};
}
} // namespace manipulators

/// Recognition vocabulary for a complete imaginary-size transcode protocol.
///
/// This concept proves only that the conversion CPO family is internally
/// coherent.  It is not, by itself, a print-emitter capability: the generic
/// freestanding printer must not accept it until an implementation actually
/// owns buffering, progress, finalization, and error semantics for the request.
template <typename char_type, typename T>
concept transcode_imaginary_protocol = ::std::integral<char_type> && requires() {
	typename T::char_type;
	typename T::transcoder_value_type;
	requires(::std::same_as<T, ::fast_io::manipulators::basic_transcoder_t<typename T::char_type, typename T::transcoder_value_type>> &&
			 (sizeof(typename T::transcoder_value_type::from_char_type) == sizeof(typename T::transcoder_value_type::to_char_type) &&
			  ::fast_io::operations::decay::defines::has_transcode_decay_define<typename T::transcoder_value_type> &&
			  ::fast_io::operations::decay::defines::has_transcode_min_tosize_decay_define<typename T::transcoder_value_type> &&
			  ::fast_io::operations::decay::defines::has_transcode_imaginary_decay_define<typename T::transcoder_value_type>)) ||
				((sizeof(typename T::transcoder_value_type::to_char_type) == 1) &&
				 ::fast_io::operations::decay::defines::has_transcode_bytes_decay_define<typename T::transcoder_value_type> &&
				 ::fast_io::operations::decay::defines::has_transcode_bytes_min_tosize_decay_define<typename T::transcoder_value_type> &&
				 ::fast_io::operations::decay::defines::has_transcode_bytes_imaginary_decay_define<typename T::transcoder_value_type>);
};

// Preserve the historical vocabulary name for code that detects the protocol.
// Print admission intentionally uses a separate emitter-capability decision.
template <typename char_type, typename T>
concept transcode_imaginary_printable = transcode_imaginary_protocol<char_type, T>;

} // namespace fast_io
