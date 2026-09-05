# Compile-resource optimization, 2026-09-05

## Outcome and limits

The library changes reduce the cost of an **existing** semantic barrier proof;
they do not admit additional source types, change CPO priority, split a record,
or modify any runtime emitter. The large synthetic proof improves substantially.
The original uwvm2 huge-CPO problem is **not yet safely resolved**.

An experiment that added uwvm provider promises made the main translation unit
compile with much less memory, but stricter differential testing rejected it:
reserve descriptor grouping changed, and a context record that previously used
one write became several writes. These promises are not installed in uwvm2 or
the library. The reproducer header is explicitly gated and the runner defaults
to no experimental promises. Its low-memory results are not accepted successes.

## Frozen inputs and environment

- Baseline library: fast_io_new `f2d37fbd99187d25adf68b9afbcc32fce391c92c`.
  “Baseline” below does **not** mean the official `../fast_io` tree.
- uwvm2: `4737560818049fb3a46a2e3f8a58ea266065da16`, captured with `git archive`
  after checking that its tracked working tree was clean. The live checkout was
  not edited. Its common fast_io headers matched the baseline library.
- Independent Linux directory:
  `/home/macromodel/Documents/src/uwvm-cpo-build.YaCpwJ`.
  `snapshot/` is the frozen uwvm2 tree; `candidate/include/` is the candidate library.
- Serial compiler processes, pinned to CPU 14, verified as a 5.6 GHz P-core.
  No compilation or benchmark was performed on the local M4.
- The host had approximately 47 GiB available RAM, but swap was already full
  and `/tmp` was an almost-full tmpfs. Sources and artifacts therefore use the
  disk directory above. Compilers were bounded to 8 or 12 GiB virtual memory;
  sanitizer executables were not given an address-space limit.
- Clang 23: `23.0.0git`, LLVM revision `4c4c1db7c69a6fda6cfa6bc6066bb09a433edc89`.
  Executable: `/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/bin/clang++`.
  Its required `LD_LIBRARY_PATH` is
  `/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/lib/x86_64-unknown-linux-gnu`.
- GCC 16: `16.0.1 20260322`, revision `r16-8246`.
- Optimization stayed at `-O3 -march=native`; no `-Ofast`, reduced optimization,
  arbitrary record splitting, or huge-CPO macro removal was used.
- uwvm main-TU probes used C++26, non-module mode, the default interpreter,
  and **disabled LLVM JIT**. They are not a full linked uwvm/JIT build validation.

## Accepted implementation

1. `print_semantic_optional_scatter_barrier_argument_v` now discards semantic
   nodes and sources lacking both provider premises before instantiating their
   irrelevant formatter protocols. The original Boolean condition is unchanged.
2. `print_semantic_optional_scatter_barrier_partition` computes segment endpoints
   once in constexpr scalar storage and applies the original segment proof to
   the exact original types. Non-final segments retain `line=false`; the final
   segment, including an empty suffix, retains original line ownership.
3. Language pack indexing requires both C++26 and `__cpp_pack_indexing >= 202311L`;
   the alternative direct indexing route requires the `__type_pack_element`
   builtin. Frontends without either keep the original
   recursive proof. GCC 11 measurements rejected simulated indexing.
4. The existing `FAST_IO_SEMANTIC_CONDITION_DIAGNOSTIC` now genuinely discards
   fallback instantiation after its failed assertion. Previously diagnostic
   recovery could continue constructing the Cartesian product and exhaust memory.
   Builds without that diagnostic macro are unaffected.

Clang 23 reported `__cpp_pack_indexing=202311L`, but did not report an expansion
statement feature macro. No unverified `template for` extension was introduced.
It also advertises pack indexing in C++20 as an extension, so the language-version
gate is needed to avoid introducing a new `-Wc++26-extensions` warning.
The final Clang 23 C++20 build passed `-Werror=c++26-extensions` and retained
the same executable hash as the baseline.

## Resource measurements

`semantic_barrier_partition_compile.cc` instantiates only the admission proof.
Each sixteen-source block has eight optional colors, seven mandatory literals,
and one distinct direct-only boundary. Thus the measurement separates proof
construction from code generation. Results include normal header parsing.

| Compiler / language | Sources | Baseline seconds / peak KiB | Candidate seconds / peak KiB |
|---|---:|---:|---:|
| Clang 23 / C++26 | 1024 | 3.60 / 582476 | 3.30 / 385400 |
| Clang 23 / C++26 | 2048 | 5.01 / 1165576 | 3.50 / 432388 |
| GCC 16 / C++26 | 1024 | 1.90 / 677244 | 1.70 / 546012 |
| GCC 11 / C++20, retained fallback | 1024 | 2.00 / 746024 | 2.20 / 745796 |

The 2048-source Clang case reduced peak RSS by **62.9%** and elapsed time by
**30.1%**. These are individual bounded samples, not statistical confidence
intervals. No speed improvement is claimed for the retained GCC 11 fallback.

An ordinary mixed runtime contract is a useful small-input control:

| Compiler / C++20 | Baseline seconds / peak KiB | Candidate seconds / peak KiB |
|---|---:|---:|
| GCC 11 | 10.62 / 986680 | 10.62 / 985384 |
| GCC 16 | 12.62 / 1093444 | 12.52 / 1095580 |
| Clang 23 | 9.21 / 820904 | 9.31 / 817924 |

These controls show no material compile-resource win; their executables are
byte-for-byte identical. The optimization is targeted at large proof graphs.

## Correctness, strategy, and code generation

- The extended `semantic_optional_scatter_plan.cc` compares the new partition
  with a separately retained reference transition system: three destinations,
  both line policies, empty records, leading/adjacent/trailing boundaries,
  insufficient mandatory leaves, const and parameter transports, unmarked
  sources, and width boundaries. It passed Clang 23, GCC 16, and GCC 11 builds.
- The existing runtime fixture passed before/after builds with GCC 11, GCC 16,
  and Clang 23. Entire executable hashes matched for each compiler:

  - GCC 11: `24a7dc069130f08945a067123cb918b4de0ef34eff6e8e7779242601ad7b26e1`
  - GCC 16: `0072d15c5aa2009d132e6d355db1498cdee9d681bbc548ca31a6e34d099533f5`
  - Clang 23: `6b941116bfaa7d34e9209039273123ad49a3f8a268f44b66cf1188c4edc1208b`

- `uwvm_compile_contract.cc`, with experimental promises **disabled**, tests
  actual uwvm direct, reserve, and context objects across all sixteen independent
  condition masks and both line policies. It includes valid and null memory
  views, large integer limits, and long/short/empty function signatures. The
  full byte/operation/descriptor transcript matched before and after, as did
  the executable hash:
  `59ce407bd6882e3b24e3df721c181736ec18abdc953d7288df1d891c398fed66`.
- Separate `-O1 -g -fno-omit-frame-pointer -fsanitize=address,leak,undefined`
  runs passed the extended semantic test and the real uwvm contract fixture,
  with leak detection enabled and UBSan configured to halt on error.
- Compile compatibility probes passed GCC 11–16 and Clang 17–23. Clang 17/18
  required `--gcc-install-dir=/usr/lib/gcc/x86_64-linux-gnu/12`: their default
  discovery selected libstdc++ 16, whose newer builtin requirements are not
  supported by these older Clang releases. No library workaround was added for
  that toolchain mismatch.
- Full uwvm main-object comparison with the **same experimental declarations on
  both sides** also matched exactly: 10030104 bytes, SHA-256
  `b2b03ebaed2e55e8f9da5452d8aa2f618d673910e2247749900c48b4613b7c96`.
  This isolates the proof-representation change; it does not validate those
  declarations against unmodified uwvm behavior.

Identical executables provide stronger evidence of unchanged generated runtime
code for these fixtures than a noisy throughput sample. No new runtime speedup
is claimed from this compile-only change.

## Rejected uwvm experiment and remaining work

Without extra declarations, the frozen huge-CPO main-TU compile reached the
12 GiB address-space ceiling after 70.38 seconds, at 12225244 KiB peak RSS, and
reported an LLVM allocation failure. The user's reported 32 GiB consumption
was deliberately not reproduced without a limit.

Declarations identifying selected uwvm direct, reserve, and context providers
made a syntax-only compile complete in 15.92 seconds / 2594720 KiB. Main-object
compilation completed around 53–57 seconds / 3.2–3.3 GiB. However:

- A direct `print_memory` record changed a scatter prefix from two descriptors
  of lengths `[2,4]` to five descriptors `[2,1,1,1,1]` for the same bytes.
- A short function-signature context record changed from one contiguous write
  to a scatter write plus separate context and suffix writes.

The second observation disproves the proposed context-boundary promise. The
first also violates the intended preservation of existing descriptor grouping.
Neither may be accepted as a compile-time optimization under the requested
strategy constraint. The next substantive fix needs a linear representation
that preserves ordinary context capture and reserve-run grouping; it cannot
be obtained by simply marking those leaves as independent boundaries.

`uwvm_compile_proofs.h` is retained solely as a **rejected experiment**, guarded
by `FAST_IO_UWVM_EXPERIMENTAL_PROOFS=1`. It is not included by public headers.
The runner requires both `PROVIDERS=1` and `ALLOW_UNVERIFIED_PROVIDERS=1` before
using it. Do not deploy it or count its low memory figures as a completed fix.

## Reproduction

On SSH Linux, from the independent directory above:

```sh
export LD_LIBRARY_PATH=/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/lib/x86_64-unknown-linux-gnu
CXX=/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/bin/clang++
taskset -c 14 "$CXX" -std=c++26 -O3 -march=native \
  -fbracket-depth=4096 -ftemplate-depth=4096 \
  -Icandidate/include -DFAST_IO_COMPILE_PACK=2048 -fsyntax-only \
  semantic_barrier_partition_compile.cc
```

Change only the include root to `snapshot/third-parties/fast_io/include` for
the baseline. Use `/usr/bin/time -v`, a timeout, and `prlimit --as` for resource
measurement as in the supplied runner.

`run_uwvm_compile.sh SNAPSHOT FAST_IO_INCLUDE DISK_BUILD_DIR` records the compiler,
command, diagnostics, resource usage, and object hash in a fresh directory.
`CXX` and a verified P-core `CPU` are required. Its default huge-CPO run remains
a bounded reproduction of the unresolved case. `PHASE=syntax DIAGNOSTIC=8`
requests the intentionally failing, bounded shape diagnostic instead.
The final no-promises diagnostic stopped at the expected assertion in 14.42
seconds / 1536808 KiB, without continuing into the rejected condition product.

To reproduce the rejected policy experiment, build `uwvm_compile_contract.cc`
twice with identical flags and compare its complete stdout: first with
`FAST_IO_UWVM_PROVIDER_PROOFS=0`, then with both
`FAST_IO_UWVM_PROVIDER_PROOFS=1` and `FAST_IO_UWVM_EXPERIMENTAL_PROOFS=1`.
The comparison is expected to fail; output text alone is not a sufficient oracle.

No commits or pushes were performed. Unrelated existing workspace files were
preserved, and the live uwvm2 checkout was not modified.
