# Scatter-chain prefetch benchmark

The measurements which constrain the current production threshold are retained in `RESULTS.md`; this file describes
the harness and the proof obligations needed to reproduce or extend them.

`MANUAL_EVIDENCE.md` records how official Intel, AMD, and Arm documentation narrows the candidate strategies without
being mistaken for runtime proof.  In particular, an ISA or tune capability may exist while every production call
site for that capability remains disabled pending measurements on the actual processor family.

This directory is an isolated evidence harness for a deliberately narrow policy: while copying one sufficiently
large scatter segment, issue an L1/keep hint for the next nonempty discontinuous segment.  The read experiment copies a
scatter source into one contiguous destination and hints the next source.  The mirrored write experiment copies one
contiguous source into scatter destinations and hints the next destination.  The baseline performs the same real
`memcpy` operations without the lookahead search or hint.

The benchmark calls the public `fast_io::prfch` primitive directly.  It intentionally does not require the production
platform allow-list: this experiment is evidence used to decide which tune families, segment thresholds, and sites may
enter that allow-list.  A real print, concat, or scan strategy must additionally satisfy its platform concept and an
explicit cacheable-memory provenance concept.  Scatter syntax, a raw pointer, and writable buffer cursors alone do not
prove that an address is ordinary cacheable RAM.

## Memory and lifetime proof

Every hinted range in a timed fixture belongs to a 64-byte-aligned allocation owned by that fixture.  Allocations are
made before measurement and are never resized or released until all samples and correctness checks finish.  The sparse
preflight uses equivalently aligned automatic arrays whose lifetime encloses both calls.  A descriptor with zero length
is skipped before its base is examined; the preflight deliberately represents such a descriptor with a null base.  The
lookahead therefore passes `prfch` only the base of a nonempty live object.  It does not form a one-past or speculative
offset pointer.

`contiguous` descriptors name adjacent payloads.  `discontinuous` descriptors name page-separated slots visited in a
deterministically shuffled order.  The latter layout supplies latency and defeats simple next-line prediction without
depending on allocator placement.  In both layouts the contiguous side is left to the hardware prefetcher.  This is
important: adding hints to both sides would obscure whether software prefetch helped the irregular side and would no
longer model the proposed concept strategy.

## Measurement boundary and negative controls

Each process runs an untimed sparse-chain preflight, including a null/zero-length descriptor between two live
segments.  Baseline and hinted kernels then run in alternating order across samples.  Exact byte validation is
performed after every timed pair, and the reported FNV-1a checksum is recomputed after all samples.  Compiler barriers
and noinline exported kernels keep the true memory writes observable without adding a fake syscall or checksum to the
measured loop.

The required negative controls are:

- 16- and 64-byte payloads, which are below the default 256-byte lead-work threshold and should emit no hint;
- hot contiguous payloads, where hardware prefetch and cache residency should make software lookahead neutral or
  harmful;
- hot discontinuous payloads, which expose the instruction/branch overhead even when the addresses are irregular but
  resident; and
- cold contiguous payloads, where a gain would require scrutiny because the continuous stream already has the most
  favorable hardware-detection shape.

`cold` mode walks an eviction buffer outside every timed operation.  Set `COLD_BYTES` to at least twice the effective
LLC available to the selected core.  Cache replacement is not an architectural guarantee, so cold results must be
reported as cache-pressure evidence rather than proof of a particular cache state.  Very small cold workloads are also
timer-dominated; they remain useful negative controls, not headline throughput numbers.

## Linux build and run

All generated files default to `/tmp/fast_io_prfch_bench`.  The script refuses to select a CPU automatically because
topology, scheduler policy, and permitted CPU sets are machine-specific.  Choose one currently idle, permitted logical
CPU and pass its number explicitly.  When `lscpu -e` exposes an SMT sibling for that CPU, leave the sibling unused for
the complete build and measurement; a machine without SMT needs no synthetic sibling restriction:

```sh
CPU=5 benchmark/0022.prfch/run_linux.sh > /tmp/fast_io_prfch_bench/results.csv
```

The default matrix covers descriptor counts 2, 8, 32, 128, and 1024; payloads 16, 64, 256, 512, and 4096 bytes; both
directions; both layouts; and hot/cold cache conditions.  Every setting is overridable without editing the source:

```sh
CPU=6 COUNTS='32 128' CACHE_MODES='hot' SAMPLES=11 \
  TARGET_BYTES_PER_SAMPLE=268435456 benchmark/0022.prfch/run_linux.sh
```

The CSV `prefetch_over_baseline` column is the median of paired per-sample ratios, not the ratio of two independently
rounded medians.  A value below one favors the hinted kernel.  Retain individual repeated runs as well as medians when
making a policy decision; reject an optimization that measurably regresses the hot negative controls.

## Portable Linux CPU snapshot

`run_cpu_snapshot_linux.sh` packages one intentionally small cross-machine comparison.  It accepts `CPU`, `CXX`, and
an absolute `OUT_DIR` below `/tmp`, records the host/compiler context, then invokes `run_linux.sh` with a fixed matrix:
32 discontinuous read sources, 4- and 16-KiB payloads, and both hot and cold conditions.  The resulting CSV therefore
contains one header and four data rows.  Compilation, preprocessing probes, and benchmark processes all inherit the
same one-CPU `taskset` affinity.

```sh
CPU=5 CXX=/usr/bin/g++ \
OUT_DIR=/tmp/fast_io_prfch_snapshot \
  benchmark/0022.prfch/run_cpu_snapshot_linux.sh
```

The output directory contains `uname.txt`, `lscpu.txt`, `lscpu_extended.txt`, `affinity.txt`, `compiler.txt`,
`target_macros.txt`, `manifest.txt`, `results.csv`, the build tree, and the compiler/runner diagnostics in
`run.stderr.log`.  `CXX` names one executable rather than a shell command with embedded arguments.  The default
snapshot uses 11 samples, three warm-ups, 256 MiB of hot work per sample, and a 128-MiB cold scrub.  These resource
controls can be reduced for a smoke test without changing the fixed comparison matrix:

```sh
CPU=5 CXX=g++ OUT_DIR=/tmp/fast_io_prfch_smoke \
SAMPLES=1 WARMUPS=1 TARGET_BYTES_PER_SAMPLE=1048576 COLD_BYTES=1048576 \
  benchmark/0022.prfch/run_cpu_snapshot_linux.sh
```

### Existing AWS machines

Run the snapshot only after the user has provisioned and connected to an instance they are authorized to use.  The
same interface covers an Intel, AMD, or Graviton Linux host; use the compiler installed on that host and give each
architecture a separate output directory:

```sh
# Existing AWS Intel Linux instance
CPU=2 CXX=/usr/bin/g++ OUT_DIR=/tmp/prfch_aws_intel \
  benchmark/0022.prfch/run_cpu_snapshot_linux.sh

# Existing AWS AMD Linux instance
CPU=2 CXX=/usr/bin/g++ OUT_DIR=/tmp/prfch_aws_amd \
  benchmark/0022.prfch/run_cpu_snapshot_linux.sh

# Existing AWS Graviton Linux instance
CPU=1 CXX=/usr/bin/g++ OUT_DIR=/tmp/prfch_aws_graviton \
  benchmark/0022.prfch/run_cpu_snapshot_linux.sh
```

Inspect `lscpu -e=CPU,CORE,SOCKET,NODE,ONLINE,MAXMHZ` before choosing `CPU`; do not infer topology from those example
numbers.  Keep an exposed SMT sibling idle and avoid concurrent compilation or memory-heavy activity on the instance.
The script does not call AWS, GCP, Bencher, Terraform, or any other provisioning/upload API; it does not discover
credentials, create or stop instances, select paid machine types, or upload results.  Provisioning, authorization,
cost approval, result collection, and instance teardown remain explicit user responsibilities.

## Code-shape inspection

The four kernels have stable C linkage names.  On Linux, build and disassemble them with the identical threshold and
target flags used by the benchmark:

```sh
CPU=5
taskset -c "${CPU}" make -C benchmark/0022.prfch -j1 ROOT="$PWD" \
  BUILD_DIR=/tmp/fast_io_prfch_bench codegen
```

Inspect the complete kernels, not merely the source line containing `prfch`.  The relevant cost includes finding the
next nonempty descriptor, the size gate, register pressure, and any changed `memcpy` lowering.  For `llvm-mca`, extract
one kernel from compiler-generated assembly and preserve the target CPU used for the runtime sample.  Instruction
prefetch is intentionally absent: these data-copy loops provide no portable, sufficiently early code-address lead.

## Public concat/print policy regression

`public_codegen_probe.cc` verifies the production dispatch boundary which the isolated kernels above intentionally
bypass. It calls only `concat_fast_io` and `print`; it never forces an internal helper's prefetch template argument.
The proved build uses the explicit cacheable scatter transport, while the otherwise identical unproved build uses a
raw scatter. Consequently this check covers entry aliasing, ABI decay, provenance propagation, the native tune
classifier, the run-time size gate, and the public concat/print materializer selection together.

Run the check on a genuine GCC hybrid target with one explicitly selected idle CPU. Compiler processes are also bound
to that CPU, and every assembly artifact is written below `/tmp` by default:

```sh
CPU=26 CXX=/usr/bin/g++-15 \
  benchmark/0022.prfch/check_public_codegen_linux.sh
```

The GCC 15/hybrid-specific expected static code shape is one `prefetcht0` in concat's next-nonempty loop and 31
`prefetcht0` instructions for the 31 edges between print's 32 variadic sources. Both unproved builds must contain zero.
The exact count is a retained compiler/code-shape contract, not an ISA-wide promise: the script deliberately refuses a
different GCC major, generic or non-hybrid tuning, Clang, and non-Linux targets rather than interpreting a validly
different lowering or a correctly disabled production policy as a failure. `-fno-prefetch-loop-arrays` prevents GCC's
optional automatic loop prefetching from being miscounted as an explicit fast_io hint.
