// Isolates the real uwvm direct formatter inside a growing conditional diagnostic record.
// Compile only, changing FAST_IO_COMPILE_CONDITIONS and the fast_io include root for A/B measurements.
#include <fast_io.h>
#include <uwvm2/parser/wasm/standard/wasm1p1/impl.h>
#include <uwvm2/uwvm/utils/memory/print.h>

#ifndef FAST_IO_COMPILE_CONDITIONS
#define FAST_IO_COMPILE_CONDITIONS 8
#endif

void uwvm_condition_compile(unsigned mask, ::uwvm2::uwvm::utils::memory::print_memory const &source)
{
	::fast_io::io::perr(::fast_io::u8err(),
#if FAST_IO_COMPILE_CONDITIONS > 0
						::fast_io::mnp::cond((mask & (1u << 0)) != 0u, u8"\033[31m"), u8"field0: ",
#endif
#if FAST_IO_COMPILE_CONDITIONS > 1
						::fast_io::mnp::cond((mask & (1u << 1)) != 0u, u8"\033[32m"), u8"field1: ",
#endif
#if FAST_IO_COMPILE_CONDITIONS > 2
						::fast_io::mnp::cond((mask & (1u << 2)) != 0u, u8"\033[33m"), u8"field2: ",
#endif
#if FAST_IO_COMPILE_CONDITIONS > 3
						::fast_io::mnp::cond((mask & (1u << 3)) != 0u, u8"\033[34m"), u8"field3: ",
#endif
#if FAST_IO_COMPILE_CONDITIONS > 4
						::fast_io::mnp::cond((mask & (1u << 4)) != 0u, u8"\033[35m"), u8"field4: ",
#endif
#if FAST_IO_COMPILE_CONDITIONS > 5
						::fast_io::mnp::cond((mask & (1u << 5)) != 0u, u8"\033[36m"), u8"field5: ",
#endif
#if FAST_IO_COMPILE_CONDITIONS > 6
						::fast_io::mnp::cond((mask & (1u << 6)) != 0u, u8"\033[37m"), u8"field6: ",
#endif
#if FAST_IO_COMPILE_CONDITIONS > 7
						::fast_io::mnp::cond((mask & (1u << 7)) != 0u, u8"\033[31m"), u8"field7: ",
#endif
#if FAST_IO_COMPILE_CONDITIONS > 8
						::fast_io::mnp::cond((mask & (1u << 8)) != 0u, u8"\033[32m"), u8"field8: ",
#endif
#if FAST_IO_COMPILE_CONDITIONS > 9
						::fast_io::mnp::cond((mask & (1u << 9)) != 0u, u8"\033[33m"), u8"field9: ",
#endif
#if FAST_IO_COMPILE_CONDITIONS > 10
						::fast_io::mnp::cond((mask & (1u << 10)) != 0u, u8"\033[34m"), u8"field10: ",
#endif
#if FAST_IO_COMPILE_CONDITIONS > 11
						::fast_io::mnp::cond((mask & (1u << 11)) != 0u, u8"\033[35m"), u8"field11: ",
#endif
#if FAST_IO_COMPILE_CONDITIONS > 12
						::fast_io::mnp::cond((mask & (1u << 12)) != 0u, u8"\033[36m"), u8"field12: ",
#endif
#if FAST_IO_COMPILE_CONDITIONS > 13
						::fast_io::mnp::cond((mask & (1u << 13)) != 0u, u8"\033[37m"), u8"field13: ",
#endif
#if FAST_IO_COMPILE_CONDITIONS > 14
						::fast_io::mnp::cond((mask & (1u << 14)) != 0u, u8"\033[31m"), u8"field14: ",
#endif
#if FAST_IO_COMPILE_CONDITIONS > 15
						::fast_io::mnp::cond((mask & (1u << 15)) != 0u, u8"\033[32m"), u8"field15: ",
#endif
						source, u8"\n");
}
