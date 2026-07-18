#pragma once

namespace fast_io_i18n
{

inline constexpr ::fast_io::i18n_module_v1::export_descriptor module_export_descriptor{
	.magic = ::fast_io::i18n_module_v1::export_magic,
	.version = ::fast_io::i18n_module_v1::export_version,
	.descriptor_size = static_cast<::std::uint_least32_t>(
		sizeof(::fast_io::i18n_module_v1::export_descriptor)),
	.abi_layout_tag = ::fast_io::i18n_module_v1::layout_tag(),
	.locale = {&lc_all_global, &wlc_all_global, &u8lc_all_global, &u16lc_all_global, &u32lc_all_global}};

static_assert(sizeof(::fast_io::i18n_module_v1::export_descriptor) <= UINT_LEAST32_MAX);

// v1 exports a pointer to immutable POD data described by the shared ABI
// header.  Returning the descriptor through an output parameter preserves the
// historical fastcall convention on 32-bit Windows while still making the
// pointee type part of the compile-time contract.
extern "C"
#if (defined(_WIN32) || defined(__CYGWIN__)) && !defined(__WINE__)
#if __has_cpp_attribute(__gnu__::__dllexport__)
	[[__gnu__::__dllexport__]]
#else
	__declspec(dllexport)
#endif
#if __has_cpp_attribute(__gnu__::__fastcall__)
	[[__gnu__::__fastcall__]]
#endif
#endif
	void
#if (defined(_WIN32) || defined(__CYGWIN__)) && !defined(__WINE__)
#if !__has_cpp_attribute(__gnu__::__fastcall__)
	__fastcall
#endif
#endif
	fast_io_i18n_export_v1(::fast_io::i18n_module_v1::export_descriptor const **result) noexcept
{
	*result = __builtin_addressof(module_export_descriptor);
}

// Keep the old symbol for binaries built against the original direct-pointer
// module contract.  New loaders never call it: v0 has no version/layout proof
// and its output type was historically confused with the unrelated public
// owning representation.
extern "C"
#if (defined(_WIN32) || defined(__CYGWIN__)) && !defined(__WINE__)
#if __has_cpp_attribute(__gnu__::__dllexport__)
	[[__gnu__::__dllexport__]]
#else
	__declspec(dllexport)
#endif
#if __has_cpp_attribute(__gnu__::__fastcall__)
	[[__gnu__::__fastcall__]]
#endif
#endif
	void
#if (defined(_WIN32) || defined(__CYGWIN__)) && !defined(__WINE__)
#if !__has_cpp_attribute(__gnu__::__fastcall__)
	__fastcall
#endif
#endif
	export_v0(lc_locale *result) noexcept
{
	*result = module_export_descriptor.locale;
}

} // namespace fast_io_i18n
