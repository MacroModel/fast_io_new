# Windows IPC benchmark

`ipc_bench.cc` compares the Windows IPC transports exposed by `fast_io` with one protocol and one output format.

Measured transports:

- `nt_thread`: two `nt_pipe` objects, with the peer running in another thread.
- `win32_thread`: two `win32_pipe` objects, with the peer running in another thread.
- `nt_process`: two `nt_pipe` objects inherited by an `nt_process` child.
- `win32_process`: two `win32_pipe` objects inherited by a `win32_process` child.
- `named_pipe_thread` and `named_pipe_process`.
- `alpc_async_thread` and `alpc_async_process`: native queued ALPC send/receive. IOCP is not used for data transfer.
- `alpc_sync_thread`: native synchronous ALPC request/reply.

The default run measures:

1. Ping-pong round-trip latency at 1, 8, 64, 256, 1024, 4096, and 16384 bytes. It reports median, p95, p99, and a clearly labelled `median / 2` one-way estimate.
2. One-way throughput and message rate. The timed interval ends only after the receiver sends an acknowledgement.
3. Duplex pipe creation/destruction, process spawn/exit, and named-pipe/ALPC connect/close setup cost.

Payload buffers and result storage are allocated before timed loops. Stream pipes use exact byte reads and writes; ALPC uses one native message per logical operation and reuses the receive object. Every case validates the final payload and protocol acknowledgement.

## Running

Use a release build for meaningful numbers:

```text
ipc_bench.exe
ipc_bench.exe --smoke
ipc_bench.exe --csv > ipc-results.csv
ipc_bench.exe --transport=nt_process,named_pipe_process,alpc_async_process
ipc_bench.exe --latency-only --iterations=20000 --warmup=2000
ipc_bench.exe --throughput-only --throughput-bytes=134217728
ipc_bench.exe --sizes=8,64,1024,4096,16384
```

Run each configuration several times on an otherwise idle machine. Pinning processes/threads and controlling CPU power state should be done by the benchmark runner when publication-quality results are required; the executable does not silently change system scheduling policy.

ALPC cases larger than `alpc_max_message_size()` are skipped. A zero-byte payload is intentionally omitted because anonymous byte-stream pipes do not represent a zero-byte write as a message, so it would not be an equivalent cross-transport test.

The named-pipe and ALPC thread connect/close setup rows include peer-thread scheduling. Process spawn/exit is reported separately, and the process variants also report a full child start + connect + close row. Steady-state process IPC latency and throughput do not include startup time.
