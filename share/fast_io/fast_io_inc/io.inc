export namespace fast_io
{

inline namespace io
{
using ::fast_io::io::print;
using ::fast_io::io::println;
using ::fast_io::io::perr;
using ::fast_io::io::perrln;
using ::fast_io::io::panic;
using ::fast_io::io::panicln;
using ::fast_io::io::scan;
#if defined(__cpp_impl_coroutine) && __has_include(<coroutine>)

using ::fast_io::io::transmit;
#endif

#ifndef FAST_IO_DISABLE_DEBUG_PRINT
using ::fast_io::io::debug_print;
using ::fast_io::io::debug_println;
using ::fast_io::io::debug_perr;
using ::fast_io::io::debug_perrln;
#endif
} // namespace io

#if defined(__cpp_impl_coroutine) && __has_include(<coroutine>)
namespace async
{
using ::fast_io::async::basic_async;
using ::fast_io::async::async;
using ::fast_io::async::annotated_operand;
using ::fast_io::async::async_operation;
using ::fast_io::async::async_printable;
using ::fast_io::async::async_scannable;
using ::fast_io::async::async_print_producer;
using ::fast_io::async::async_scan_consumer;
using ::fast_io::async::async_print_materializable;
using ::fast_io::async::materialize_at_issue;
using ::fast_io::async::async_scan_reusable;
using ::fast_io::async::borrow_scan_target;
using ::fast_io::async::argument_point;
using ::fast_io::async::argument_subpoint;
using ::fast_io::async::value_dependency;
using ::fast_io::async::value_points;
using ::fast_io::async::value_checkpoint;
using ::fast_io::async::value_print_step;
using ::fast_io::async::value_scan_step;
using ::fast_io::async::value_eof_step;
using ::fast_io::async::point_dependency;
using ::fast_io::async::point_dependencies;
using ::fast_io::async::point_precedes;
using ::fast_io::async::valid_observable_order;
using ::fast_io::async::complete_point;
using ::fast_io::async::unused_point;
using ::fast_io::async::failed_point;
using ::fast_io::async::observable_point;
using ::fast_io::async::point_state;
using ::fast_io::async::point_side;
using ::fast_io::async::point_kind;
using ::fast_io::async::file_points;
using ::fast_io::async::file_capabilities;
using ::fast_io::async::wrapped_file_points;
using ::fast_io::async::input_point;
using ::fast_io::async::output_point;
using ::fast_io::async::byte_completion;
using ::fast_io::async::direct_transfer;
using ::fast_io::async::transfer_completion;
using ::fast_io::async::byte_io_awaiter;
using ::fast_io::async::immediate_input;
using ::fast_io::async::immediate_output;
using ::fast_io::async::immediate_output_range;
using ::fast_io::async::immediate_output_reserve;
using ::fast_io::async::immediate_input_scan;
using ::fast_io::async::parse_code_input_scan;
using ::fast_io::async::snapshot_printable;
using ::fast_io::async::input_device;
using ::fast_io::async::io_device;
using ::fast_io::async::native_error;
using ::fast_io::async::native_input;
using ::fast_io::async::native_output;
using ::fast_io::async::output_device;
using ::fast_io::async::task;
}
#endif

} // namespace fast_io
