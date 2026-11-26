#pragma once

#if defined(__HERBCEPTIONS__)
#define FAST_IO_ASYNC_THROWS throws
#else
#define FAST_IO_ASYNC_THROWS
#endif

#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && !defined(_LIBCPP_FREESTANDING)) || defined(FAST_IO_ENABLE_HOSTED_FEATURES)) && !defined(__AVR__)
#if defined(__linux__) && __has_include(<poll.h>) && defined(__NR_read) && defined(__NR_write) && defined(__NR_close)
#define FAST_IO_ASYNC_LINUX
#define FAST_IO_ASYNC_POSIX
#include <poll.h>
#if __has_include(<linux/io_uring.h>) && __has_include(<sys/mman.h>) && defined(__NR_io_uring_enter) && defined(__NR_io_uring_setup) && !defined(FAST_IO_DISABLE_IO_URING)
#include <linux/io_uring.h>
#if defined(IORING_FEAT_RW_CUR_POS)
#define FAST_IO_ASYNC_URING
#if defined(__NR_pipe2) && defined(__NR_splice) && defined(__NR_statx) && defined(F_SETPIPE_SZ) && defined(IORING_FEAT_FAST_POLL) && !defined(FAST_IO_DISABLE_LINUX_SPLICE)
#define FAST_IO_ASYNC_SPLICE
#endif
#include <sys/mman.h>
#include <atomic>
#endif
#endif
#if __has_include(<sys/epoll.h>) && (defined(__NR_epoll_create1) || defined(__NR_epoll_create)) && defined(__NR_epoll_ctl) && (defined(__NR_epoll_wait) || defined(__NR_epoll_pwait)) && !defined(FAST_IO_DISABLE_EPOLL)
#define FAST_IO_ASYNC_EPOLL
#include <sys/epoll.h>
#endif
#if __has_include(<sys/eventfd.h>)
#include <sys/eventfd.h>
#define FAST_IO_ASYNC_EVENTFD
#endif
#elif !defined(_WIN32) && !defined(__MSDOS__) && !defined(__wasi__) && defined(_POSIX_VERSION)
#define FAST_IO_ASYNC_POSIX
#if __has_include(<sys/event.h>) && __has_include(<poll.h>) && !defined(FAST_IO_DISABLE_KQUEUE)
#include <sys/event.h>
#include <poll.h>
#if defined(EVFILT_READ) && defined(EVFILT_WRITE)
#define FAST_IO_ASYNC_KQUEUE
#endif
#endif
#elif defined(_WIN32) && !defined(__WINE__) && !defined(__CYGWIN__)
#if defined(_WIN32_WINDOWS)
#define FAST_IO_ASYNC_WIN9X
#else
#define FAST_IO_ASYNC_NT
#endif
#endif

#if defined(FAST_IO_ASYNC_POSIX) && !defined(FAST_IO_ASYNC_EPOLL) && !defined(FAST_IO_ASYNC_URING)
#if __has_include(<sys/select.h>)
#include <sys/select.h>
#elif __has_include(<sys/time.h>)
#include <sys/time.h>
#endif
#if defined(FD_SETSIZE) && defined(FD_ZERO)
#define FAST_IO_ASYNC_SELECT
#endif
#endif

#if defined(FAST_IO_ASYNC_POSIX) && !defined(__SINGLE_THREAD__) && !defined(__NEWLIB__) && !defined(FAST_IO_DISABLE_ASYNC_WORKERS) && __has_include(<pthread.h>) && (!defined(_POSIX_THREADS) || _POSIX_THREADS >= 0) && (defined(FAST_IO_ASYNC_LINUX) || defined(FAST_IO_ASYNC_SELECT))
#define FAST_IO_ASYNC_WORKERS
#include <pthread.h>
#include "../threads/mutex/posix_pthread_mutex.h"
#endif
#endif

#if defined(FAST_IO_ASYNC_WORKERS) && defined(FAST_IO_ASYNC_SPLICE) && defined(__NR_copy_file_range) && defined(__NR_sched_getaffinity) && defined(__NR_sched_setaffinity)
#define FAST_IO_ASYNC_FILE_PLACEMENT
#if defined(__x86_64__) && !defined(__ILP32__) && defined(__NR_fstat) && defined(__NR_sendfile) && defined(__NR_fstatfs) && __has_include(<sys/statfs.h>)
#include <sys/statfs.h>
// A regular-file job can retain both file positions until EOF. Ordinary descriptors
// continue to submit through the ring without carrying a linked-splice request group.
#define FAST_IO_ASYNC_FILE_WORKER
#undef FAST_IO_ASYNC_SPLICE
#endif
#endif

#if defined(FAST_IO_ASYNC_POSIX)
#include "posix_request.h"
#endif
#if defined(FAST_IO_ASYNC_LINUX)
#include "linux_poll.h"
#endif
#if defined(FAST_IO_ASYNC_LINUX) && (defined(FAST_IO_ASYNC_URING) || defined(FAST_IO_ASYNC_EPOLL))
#define FAST_IO_ASYNC_POLL
#include "poll.h"
#endif
#if defined(FAST_IO_ASYNC_URING)
#include "uring.h"
#endif
#if defined(FAST_IO_ASYNC_EPOLL)
#include "epoll.h"
#endif
#if defined(FAST_IO_ASYNC_SELECT)
#include "select.h"
#endif
#if defined(FAST_IO_ASYNC_KQUEUE)
#include "kqueue.h"
#endif
#if defined(FAST_IO_ASYNC_NT) || defined(FAST_IO_ASYNC_WIN9X)
#include "windows_wait.h"
#endif
#if defined(FAST_IO_ASYNC_NT)
#include "nt.h"
#endif
#if defined(FAST_IO_ASYNC_WORKERS)
#include "worker.h"
#endif
#include "context.h"
#include "awaiters.h"
#if defined(__APPLE__) && defined(FAST_IO_ASYNC_WORKERS)
#include "darwin_file_copy.h"
#endif
#if defined(__FreeBSD__) && defined(FAST_IO_ASYNC_WORKERS) && __has_include(<sys/syscall.h>)
#include <sys/syscall.h>
#if defined(SYS_copy_file_range)
#include "freebsd_file_copy.h"
#endif
#endif
#if defined(FAST_IO_ASYNC_NT)
#include <atomic>
#include <span>
#include "nt_file_copy.h"
#include "nt_positioned_read.h"
#endif
#if defined(FAST_IO_ASYNC_SPLICE)
#include "splice.h"
#endif
#if defined(FAST_IO_ASYNC_LINUX) && defined(__x86_64__) && !defined(__ILP32__) && defined(__NR_fstat)
#define FAST_IO_ASYNC_FILE_COPY
#endif
#if defined(FAST_IO_ASYNC_SPLICE) || defined(FAST_IO_ASYNC_FILE_COPY)
#if defined(FAST_IO_ASYNC_FILE_WORKER)
#include "regular_file.h"
#endif
#include "file_copy.h"
#endif
#undef FAST_IO_ASYNC_FILE_COPY

#undef FAST_IO_ASYNC_POSIX
#undef FAST_IO_ASYNC_POLL
#undef FAST_IO_ASYNC_LINUX
#undef FAST_IO_ASYNC_SELECT
#undef FAST_IO_ASYNC_EPOLL
#undef FAST_IO_ASYNC_SPLICE
#undef FAST_IO_ASYNC_URING
#undef FAST_IO_ASYNC_FILE_WORKER
#undef FAST_IO_ASYNC_FILE_PLACEMENT
#undef FAST_IO_ASYNC_WORKERS

#undef FAST_IO_ASYNC_KQUEUE
#undef FAST_IO_ASYNC_WIN9X
#undef FAST_IO_ASYNC_NT
#undef FAST_IO_ASYNC_EVENTFD
#undef FAST_IO_ASYNC_THROWS
