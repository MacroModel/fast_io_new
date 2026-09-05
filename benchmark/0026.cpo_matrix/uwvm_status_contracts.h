#pragma once

#if !defined(FAST_IO_UWVM_STATUS_AUDIT) || FAST_IO_UWVM_STATUS_AUDIT != 1
#error "Enable only after auditing the pinned uwvm2 snapshot described in this header"
#endif

#include <fast_io.h>
#include <uwvm2/parser/wasm/standard/wasm1/features/def.h>

// Integration proof for uwvm2 4737560818049fb3a46a2e3f8a58ea266065da16 and this fast_io tree, Linux non-module builds.
// A full source search of this snapshot's src, bizwen and boost_unordered trees found no status_print_define.
// Only the exact library-owned leaves and built-in uwvm types below are admitted. Feature extension types are excluded.
// The exact native POSIX observer owns no competing whole-record status operation. Re-audit this declaration if either
// source tree changes: this is a complete-record semantic promise, not an inference from structural printability.
// This header makes NO optional-scatter leaf, barrier, context-barrier, or reserve grouping declarations. The new
// emitter must independently preserve every ordinary control strategy. The live uwvm2 checkout is never modified.

namespace uwvm2::uwvm::utils::memory
{
struct print_memory;
}
namespace uwvm2::parser::wasm::base
{
struct error_output_t;
}
namespace uwvm2::validation::error
{
struct error_output_t;
}
namespace uwvm2::parser::wasm_custom::customs
{
struct name_error_output_t;
}
namespace uwvm2::utils::cmdline::details
{
struct usage_printer;
}
namespace uwvm2::parser::wasm::standard::wasm1::type
{
struct limits_type_section_details_wrapper_t;
struct memory_type_section_details_wrapper_t;
struct table_type_section_details_wrapper_t;
struct global_type_section_details_wrapper_t;
} // namespace uwvm2::parser::wasm::standard::wasm1::type
namespace uwvm2::uwvm::wasm::type
{
struct module_memory_limit_section_details_wrapper_t;
}
namespace uwvm2::parser::wasm::standard::wasm1p1::features
{
struct wasm1p1;
struct table_type_section_details_wrapper_t;
struct global_type_section_details_wrapper_t;
} // namespace uwvm2::parser::wasm::standard::wasm1p1::features
namespace uwvm2::parser::wasm::standard::wasm1::features
{
struct wasm1;
}

namespace uwvm_status_contracts
{
template <typename T>
struct builtin_scalar : ::std::false_type
{};
template <::fast_io::manipulators::scalar_flags flags, typename T>
struct builtin_scalar<::fast_io::manipulators::scalar_manip_t<flags, T>>
	: ::std::bool_constant<::std::is_arithmetic_v<::std::remove_cvref_t<T>>>
{};

template <typename T>
struct builtin_function_signature : ::std::false_type
{};
template <::uwvm2::parser::wasm::concepts::wasm_feature... Features>
struct builtin_function_signature<
	::uwvm2::parser::wasm::standard::wasm1::features::final_function_type_section_details_wrapper_t<Features...>>
	: ::std::bool_constant<((::std::same_as<Features, ::uwvm2::parser::wasm::standard::wasm1::features::wasm1> ||
							 ::std::same_as<Features, ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1>) &&
							...)>
{};

template <typename T>
inline consteval bool source()
{
	using value_type = ::fast_io::details::decay::print_runtime_scatter_plan_unwrapped_t<T>;
	if constexpr (::fast_io::details::decay::print_semantic_top_level_condition_v<T>)
	{
		using node = ::std::remove_reference_t<decltype(::fast_io::details::decay::print_semantic_node_ref(::std::declval<T>()))>;
		return source<decltype((::std::declval<node &>().t1))>() && source<decltype((::std::declval<node &>().t2))>();
	}
	else
	{
		return ::std::same_as<value_type, ::fast_io::io_null_t> ||
			   ::fast_io::details::decay::print_semantic_optional_scatter_closed_leaf<value_type>::value ||
			   builtin_scalar<value_type>::value || ::std::is_arithmetic_v<value_type> ||
			   ::std::same_as<value_type, ::fast_io::iso8601_timestamp> ||
			   ::std::same_as<value_type, ::uwvm2::uwvm::utils::memory::print_memory> ||
			   ::std::same_as<value_type, ::uwvm2::parser::wasm::base::error_output_t> ||
			   ::std::same_as<value_type, ::uwvm2::validation::error::error_output_t> ||
			   ::std::same_as<value_type, ::uwvm2::parser::wasm_custom::customs::name_error_output_t> ||
			   ::std::same_as<value_type, ::uwvm2::utils::cmdline::details::usage_printer> ||
			   ::std::same_as<value_type, ::uwvm2::parser::wasm::standard::wasm1::type::limits_type_section_details_wrapper_t> ||
			   ::std::same_as<value_type, ::uwvm2::parser::wasm::standard::wasm1::type::memory_type_section_details_wrapper_t> ||
			   ::std::same_as<value_type, ::uwvm2::parser::wasm::standard::wasm1::type::table_type_section_details_wrapper_t> ||
			   ::std::same_as<value_type, ::uwvm2::parser::wasm::standard::wasm1::type::global_type_section_details_wrapper_t> ||
			   ::std::same_as<value_type, ::uwvm2::parser::wasm::standard::wasm1p1::features::table_type_section_details_wrapper_t> ||
			   ::std::same_as<value_type, ::uwvm2::parser::wasm::standard::wasm1p1::features::global_type_section_details_wrapper_t> ||
			   ::std::same_as<value_type, ::uwvm2::uwvm::wasm::type::module_memory_limit_section_details_wrapper_t> ||
			   builtin_function_signature<value_type>::value;
	}
}
} // namespace uwvm_status_contracts

namespace fast_io
{
template <bool line, typename... Args>
	requires((::uwvm_status_contracts::source<Args>()) && ...)
inline constexpr ::std::true_type print_semantic_status_free_record(
	::fast_io::io_reserve_type_t<char8_t, ::fast_io::basic_posix_family_io_observer<::fast_io::posix_family::api, char8_t>>,
	::std::bool_constant<line>, ::fast_io::io_type_t<Args>...) noexcept
{
	return {};
}
} // namespace fast_io
