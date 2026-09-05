# Old/new print and concat CPO matrix

This directory is a first, deliberately finite scaffold for comparing the
official `../fast_io` tree with `fast_io_new`.  The translation units are
shared verbatim: selecting a tree changes only the include root supplied to the
compiler.  Every executable contains one source family, one argument count,
one line policy, and one destination.  This prevents unrelated template
instantiations from changing inliner budgets, compiler memory, binary size, or
hot-code placement.

## Protocol set

| Name | Source contract |
|---|---|
| `f` | type-static reserve upper bound plus actual end cursor |
| `d` | object-dependent conservative reserve bound plus actual end cursor |
| `p` | dynamic fallback plus exact size/exact writer |
| `s` | one scatter descriptor, intentionally without a new-only borrowed marker |
| `bs` | one scatter descriptor with an independent-storage lifetime proof |
| `a` | value alias proxy whose sole formatter is `f` |
| `mixed` | deterministic repeating `f,d,p,s,a` type schedule |
| `mixed_b` | deterministic repeating `f,d,p,bs,a` type schedule |
| `bd` | dynamic reserve plus a non-consuming, non-fatal one-pass bound |
| `pp` | cached precise reserve with fresh-concat and output-growth proofs |
| `ss` | borrowed scatter with output-state, direct-equivalence, copy-stability, and eager-observation proofs |
| `mixed_p` | deterministic repeating `f,bd,pp,ss,a` proof-rich schedule |

The sources accept every argument count from `1` through `64`; the default
runner selects `1`, `2`, `8`, and `32`. Recommended boundary probes also use
`9`, `16`, `31`--`35`, `51`--`55`, and `64`. The adjacent `8`/`9`
pair is the explicit boundary control for long-pack code-generation policies;
`16` distinguishes a true cardinality trend from a 32-leaf outlier. The token lengths cross
`0/1/7/8/15/16/22/23`; their bytes are generated from the run-time seed before
measurement.  Source-pack tuples are then built once and passed as const
lvalues.  This keeps the source objects and their referenced corpus storage
alive for every synchronous CPO call without timing tuple construction.

Both scatter sources designate the same long-lived immutable corpus.  The `s`
type remains unmarked because a raw pointer/length descriptor is not a formal
generic proof that a dispatcher may retain or replay it.  The distinct `bs`
type publishes that proof, making `mixed` versus `mixed_b` a controlled
provenance boundary rather than silently granting stronger semantics to every
scatter producer.

The proof-rich companions deliberately keep their ordinary formatting CPOs
byte-equivalent to the controls.  Their extra ADL functions are independent
semantic premises understood only by the new planner: `bd` may be bounded
without consuming its source, `pp` has a cached destination-independent exact
extent, and `ss` supplies each scatter-purity/lifetime premise separately.
The official tree simply ignores those additional functions, so an old/new
cell changes policy admission without changing the source bytes or oracle.

`print_case.cc` supports:

- `OUTPUT=obuffer`: a public `basic_obuffer_view<char>` with enough capacity
  for all conservative bounds;
- `OUTPUT=raw`: a synthetic typed write-all observer backed by a cyclic 64 KiB
  ring.

`concat_case.cc` supports `RESULT=std` and `RESULT=fast`, invoking the public
`concat[_ln]_std` and `concat[_ln]_fast_io` APIs respectively.

## Correctness and DCE controls

Before timing, every one of the 256 corpus records is compared byte-for-byte
against an independent oracle.  The oracle reads only the original corpus and
the line flag; it never calls a fast_io alias, reserve, precise, scatter,
concat, or output CPO.  Validation includes empty records and records whose
actual length is below a reserve bound.

The timed operation publishes its complete output interval to an opaque
compiler barrier with a memory clobber.  That barrier performs no byte walk, so
formatting is not hidden under a timed checksum.  A full digest is computed
outside timing for the persistent print destination, while concat relies on
the per-result barrier and the complete untimed corpus validation before its
temporary result is destroyed.

Each process calibrates to 150 ms by default.  The accepted target range is
20--300 ms, and a process-local `ITIMER_REAL` enforces an independent 800 ms
hard boundary without a watchdog thread.  A pilot starts at 16 calls and is
extended only until it reaches approximately 1 ms.  Mutable destination state
is reset after calibration so the formal sample never inherits a
speed-dependent ring cursor or counter from the pilot.  The emitted CSV row
records the actual duration and iterations; a deadline termination invalidates
that cell instead of silently admitting an overlong sample.

## One case

From this directory, build against the new tree:

```sh
make -j1 print FAST_IO_ROOT=../.. SOURCE=mixed PACK=8 OUTPUT=obuffer
make -j1 concat FAST_IO_ROOT=../.. SOURCE=mixed PACK=8 RESULT=std
/tmp/fast_io_cpo_matrix/new/print-mixed-n8-l0-obuffer 12345 150
/tmp/fast_io_cpo_matrix/new/concat-mixed-n8-l0-std 12345 150
```

Build the exact same source against the official tree by changing only the
root and tag:

```sh
make -j1 print FAST_IO_ROOT=../../../fast_io TAG=old SOURCE=mixed PACK=8 OUTPUT=obuffer
make -j1 concat FAST_IO_ROOT=../../../fast_io TAG=old SOURCE=mixed PACK=8 RESULT=fast
```

`LINE=1` selects `println`/`concatln`; the initial runner defaults to `LINE=0`.
Set `BUILD_DIR` to give a run its own artifact directory.

## Serial old/new runner

`run_matrix.sh` compiles and runs every initial family/count combination for
obuffer, the synthetic raw sink, std::string, and fast_io::string.  It uses
`make -j1`, completes the entire serial build phase before timing, waits for a
short configurable cooldown, never starts concurrent compiler or benchmark
jobs, and alternates old/new execution order for successive pairs.  Set
`COOLDOWN_SECONDS=0` only for non-performance smoke tests.

On SSH Linux, `CPU` is mandatory unless `ALLOW_UNPINNED=1` is explicitly used
for a non-timing smoke test.  Select a verified idle P-core and leave its SMT
sibling idle:

```sh
CPU=4 CXX=g++ TARGET_MS=150 ./run_matrix.sh > /tmp/cpo-matrix.csv
```

Restricting a diagnostic run does not require editing sources:

```sh
CPU=4 SOURCES='p mixed' PACKS='1 32' PRINT_OUTPUTS=obuffer \
  CONCAT_RESULTS='std fast' ./run_matrix.sh
```

The default source list remains the compact control matrix.  Run the additional
proof lattice as a separate short batch so compiler heat from a much larger
build does not precede the control timings:

```sh
CPU=4 SOURCES='bd pp ss mixed_p' PACKS='1 8 32' ./run_matrix.sh
```

For Apple M4, all builds must remain serial and their artifacts must live
under `/tmp`.  Supply the required upstream Clang flags explicitly:

```sh
BUILD_DIR=/tmp/fast_io_cpo_matrix.m4 \
CXX=clang++ \
CXXFLAGS="--sysroot=$SYSROOT -fuse-ld=lld -march=native -O3 -std=c++20 -DNDEBUG" \
ALLOW_UNPINNED=1 ./run_matrix.sh
```

Do not run another compiler or benchmark job concurrently on M4.  The command
above is documentation for the later controlled run; creation of this scaffold
does not compile or execute it.

Sanitizer validation is a separate, non-performance pass.  Override
`CXXFLAGS` with `-O1 -g -fno-omit-frame-pointer
-fsanitize=address,leak,undefined`, reduce the selected matrix if necessary,
and do not report those timings as benchmark results.

`concat_dynamic_length_case.cc` is the orthogonal run-time-size control for a
homogeneous dynamic-reserve pack.  It accepts one token length in `0..65536`
and repeats that token in any compile-time `PACK` from `2` through `64` to
produce one concat result.  The adjacent 8/9 pair controls a possible
code-generation threshold; 16 distinguishes a monotonic cardinality effect
from a 32-leaf outlier; 31--35 surround that isolated boundary; 51--55 surround
the large-pack boundary and exercise every possible four-object terminal
chunk; and 64 detects a strategy which accidentally resumes per-leaf
expansion. This separates CPO-cardinality costs from payload-copy costs and
explicitly crosses SSO, allocator-growth, cache-line, page, and large-copy
boundaries. Compile old and new from the identical source, for example:

```sh
g++ -I../../../fast_io/include -I. -O3 -march=native -std=c++23 \
  -DFAST_IO_CPO_MATRIX_PACK=8 concat_dynamic_length_case.cc -o /tmp/concat-length-old
g++ -I../.. -I. -O3 -march=native -std=c++23 \
  -DFAST_IO_CPO_MATRIX_PACK=8 concat_dynamic_length_case.cc -o /tmp/concat-length-new
/tmp/concat-length-old 4096 20
/tmp/concat-length-new 4096 20
```

`print_std_dynamic_length_case.cc` uses the same source and length domain but
targets a persistent default `std::string`. Its untimed preflight establishes
the full result capacity, and every timed iteration calls `clear()` before
printing. The case therefore controls for allocator growth and measures the
maintained put area itself; it is the print-side companion to fresh-result
concat, not another construction benchmark.

## Ordered-staging regression runner

`run_concat_ordered_staging.py` is the bounded full regression for the
ordered one-pass concat policy. Its `main` group has exactly 16 translation
units: unretained `mixed` packs at N=7/8/9/32, plus the borrowed-scatter and
homogeneous-precise negative controls at N=8/32, each targeting both string
results. The separately labelled supplemental groups add four narrow newline
checks and three single-barrier positions. By default, every binary runs `small`, 2047,
2048, and 2049-byte payloads, so the inline/spill boundary is crossed without
adding another template instantiation.

The runner freezes the common source bytes, hashes each exact include tree,
and performs syntax, `-O3` object, and link passes for each O-N-N-O build. It
records wall time, peak RSS, object/linked text and file sizes, then performs
runtime O-N-N-O with independent full-byte validation and an old/new digest
equality check. `build.csv`, `runtime.csv`, the manifest, diagnostics, and all
artifacts remain in the printed run directory.

On Apple M4 the runner selects the repository's custom Clang 23, discovers the
active SDK with `/usr/bin/xcrun --show-sdk-path`, always supplies `--sysroot`,
uses `-march=native`, and explicitly selects `ld64.lld` by default because the
Apple linker cannot consume this LLVM build's builtins bitcode. It also rejects
any output directory outside `/tmp` and refuses runtime sampling when load or
Spotlight activity exceeds its guard. Review the finite grid without compiling
it with:

```sh
./run_concat_ordered_staging.py --list-cases
./run_concat_ordered_staging.py --dry-run --group main
```

The default profile set remains `small/2047/2048/2049`. To isolate the
adaptive destination's byte transition without creating another translation
unit, the same binary also accepts the opt-in `511/512/513` profiles:

```sh
./run_concat_ordered_staging.py \
  --case main.mixed.n8.std-string.line0.repeated \
  --profiles 511 512 513 --target-milliseconds 20
```

Larger opt-in profiles are available at `2559/2560/2561`, `4095/4096/4097`,
and `8191/8192/8193`. Run each derived batch independently, for example:

```sh
./run_concat_ordered_staging.py \
  --case main.mixed.n8.std-string.line0.repeated \
  --profiles 4095 4096 4097 --target-milliseconds 20
```

The runner defines `FAST_IO_ORDERED_STAGING_MAXIMUM_TOTAL_PAYLOAD` as the
largest selected numeric profile, with a minimum of 2049. This bounds both
the corpus storage and the fixed-source CPO's static reserve contract at
`ceil(maximum_total_payload/N)` per leaf. The default batch and the optional
`511/512/513` batch retain the original 2049 bound and code-generation path.
The actual bound is recorded in `manifest.json`, every build/runtime CSV row,
and the syntax/object compiler commands; the CSV schema is
`concat-ordered-staging-2`.

Compare old and new within the same derived batch. A `small` profile compiled
alongside an 8193-byte profile has a larger type-level reserve bound than
`small` in the default batch, so comparing those binaries is not a
single-variable payload-length experiment. Separate invocations and their
recorded bounds keep this distinction auditable.

On SSH Linux, `--p-core-cpu` is mandatory and applies to compilation, linking,
and runtime. The named CPU is checked against the process affinity and sysfs;
because Linux has no portable P/E-core label, the operator must select a
verified idle performance core and the runner records the raw `core_type` and
SMT sibling set for audit:

```sh
./run_concat_ordered_staging.py --compiler /usr/bin/g++ \
  --standard c++23 --p-core-cpu 4 --group main
```

Future batches should add status, mutex, scatter-output, context,
reserve-scatters, semantic thresholds, and new-only provenance markers in
separate translation units rather than expanding either initial case into a
mega executable.
