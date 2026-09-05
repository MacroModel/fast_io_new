# Linear control planning: Linux validation, 2026-09-05

## Scope and integration requirement

This change reduces template construction for large print records while retaining
the ordinary control selector's CPO priority, context capture, reserve grouping,
scatter provenance, query ordering, and newline ownership. Optimization remains
`-std=c++26 -O3 -march=native`; the huge-CPO macro remains enabled.

The successful full uwvm2 build uses the separately audited
[`uwvm_status_contracts.h`](uwvm_status_contracts.h), enabled explicitly by
`STATUS_CONTRACTS=1`. This is a **complete active-record status absence promise**,
not a barrier or scatter-grouping promise. It admits only a closed whitelist of
library and pinned uwvm types on the exact native POSIX `char8_t` observer. It
does not assert that custom formatters may be split, regrouped, or forwarded again.
The library still checks source-record status ownership first.

Without that promise, arbitrary ADL `status_print_define` can distinguish every
active argument pack. Exact checking remains potentially exponential; the
unannotated huge uwvm main TU still exhausted a 24 GiB address-space ceiling.
The result must therefore not be described as an unconditional, annotation-free
fix for every huge record. The proof header must be re-audited when its pinned
source universe changes.

The older `uwvm_compile_proofs.h` experiment remains rejected: it changed reserve
grouping and context behavior. It was not used for any accepted result here.
See [the preceding investigation](VALIDATION_COMPILE_MEMORY_20260905.md).

## Frozen inputs and environment

- Before-change fast_io_new: `82c6984828d3a3ebc6fbbb93f6fa2d3db65af3f6`.
- Official sibling fast_io: `1a3843dd3e34d3c9b6bb8cc2dca3e698ee5ac882`,
  independently archived into `official/` for the additional speed comparison.
- uwvm2: `4737560818049fb3a46a2e3f8a58ea266065da16`, independently archived.
  No local uwvm2 source was edited. This snapshot already uses the new fast_io API.
- Linux artifact root:
  `/home/macromodel/Documents/src/uwvm-cpo-linear.GXZozj`.
  `baseline/include` is the before-change library; `final-tail/include` is the
  final candidate. Neither is a link into a live working tree.
- `/tmp` was a nearly full tmpfs. The independent disk directory above avoids
  charging copied sources and object files against the memory under measurement.
- Clang `23.0.0git`, revision
  `4c4c1db7c69a6fda6cfa6bc6066bb09a433edc89`:
  `/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/bin/clang++`.
  `LD_LIBRARY_PATH`:
  `/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/lib/x86_64-unknown-linux-gnu`.
- Compatibility compiler: GCC `16.0.1 20260322` (`r16-8246`).
- Timed work ran serially on verified P-core CPU 14 (5.6 GHz maximum).
  Compiler resource measurements use `/usr/bin/time -v`, with explicit timeout
  and per-process virtual-memory ceilings. Peak RSS is reported separately from
  the ceiling. No benchmark ran on the local Mac.
- Full program configuration: non-module build, default interpreter, LLVM JIT
  disabled. This does not validate module builds or the LLVM JIT configuration.

## Implementation

`print_freestanding.h` now avoids constructing scanner metadata for already
consumed prefixes and singleton leaves. C++26 pack indexing skips directly to
the unconsumed argument. For packs of at least 128 sources, scanner metadata is
classified once and reduced in constexpr scalar arrays. The original recursive
scanner remains for small packs and unsupported frontends. Laziness at the
first stop, overflow resets, and trailing state are tested against a retained
independent oracle.

`print_semantic_status_probe.h` canonicalizes exact active terminal types and
removes equivalent condition choices. Its named-lvalue requires-expression
retains the original ADL lookup and parameter adjustment rules. It does not
infer status absence from individual leaf printability.

`print_semantic_linear.h` builds a fixed source graph and the original control
regions at compile time, then compacts selected closed optional leaves at run
time. The all-on/all-off projections use the ordinary emitter directly. A fully
mandatory suffix returns to the ordinary emitter only at an existing region
boundary, so context capture and mixed reserve/scatter groups are not cut.
Selected values are captured once in source order. Unsupported formatter
combinations, buffers, mutexes, proxies, and destination capabilities keep the
existing path.

The new `semantic_status_free_record` concept is only an optional admission
proof; it does not replace the independent strategy proof. GCC 16 and Clang 23
exercise the C++26 path. Clang C++20 exercises the retained fallback. No
unverified `template for` extension was needed.

## Reproduction

Prepare independent snapshot and library copies, then run:

```sh
export LD_LIBRARY_PATH=/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/lib/x86_64-unknown-linux-gnu
export CXX=/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/bin/clang++
export CPU=14
STATUS_CONTRACTS=1 TIMEOUT_SECONDS=600 \
  bash benchmark/0026.cpo_matrix/run_uwvm_compile.sh \
  /path/to/pinned/uwvm-snapshot /path/to/candidate/include /path/to/disk-artifacts
```

The runner creates a fresh directory containing compiler identity, exact command,
diagnostics, timing, RSS, and the main object. It treats an OOM diagnostic as a
failure even if Clang incorrectly exits zero. `PHASE=syntax` measures frontend
work only. `MAX_VIRTUAL_BYTES` controls the address-space ceiling.

Finish and exercise that exact successful main-object configuration with:

```sh
python3 benchmark/0026.cpo_matrix/run_uwvm_program.py \
  /path/to/disk-artifacts/compile.XXXXXX --cpu 14
```

This runner preserves all recorded compile options, adds the runtime TU's four
original floating-point flags, and links the three required translation units.
It generates independent Wasm fixtures in a fresh artifact directory. It needs
neither xmake nor wat2wasm and does not regenerate fixtures in the snapshot.

## Validation coverage

- Differential policy contracts compare complete output, primitive operation
  kinds, scatter counts and lengths, borrowed-pointer provenance, reserve queries,
  context capacities, newline ownership, overflow, and visible exception prefixes.
  Cases that newly enter the linear path are also compiled with the semantic
  condition diagnostic to verify admission.
- Typed and byte-scatter destinations cover both `char` and `char32_t`, including
  constexpr cases. The actual uwvm formatter contract covers memory, limits,
  module-memory limits, and empty/short/128-parameter function signatures.
- Exact status tests cover all masks, empty and line records, const arguments,
  parameter transports, nested conditions, normalization, hidden friends, and
  array/function parameter adjustment. Tests also ensure source-record status
  still wins when an active-record status-free proof is present.
- Clang C++20: eleven selected print-strategy tests passed.
- GCC 16 C++26: scanner, status-free, complete policy, and wide policy tests passed;
  policy transcripts match the Clang before-change oracle.
- Clang C++26 ASan/UBSan: policy, wide policy, and status-free tests passed.
  The final tail implementation was rechecked under both GCC and sanitizers;
  complete transcripts match and sanitizer stderr is empty.

## Supporting compile measurements

These are individual samples, not confidence intervals. They isolate template
proof costs and include normal header parsing.

| Fixture | Reference seconds / peak KiB | Optimized seconds / peak KiB |
|---|---:|---:|
| 1024-source scanner proof | 7.59 / 594876 | 7.21 / 493792 |
| 12-condition exact status proof | 4.86 / 534808 | 4.43 / 445556 |

The scanner row compares the before-change scanner with the indexed scanner
(`-fbracket-depth=4096 -ftemplate-depth=4096 -fconstexpr-depth=4096`). The status
row compares this investigation's initial exact recursive proof with the final
canonical algorithm, not the shipped before-change library, which had no
standalone status-proof header. Those syntax-only status runs both used `-O3`
without `-march=native`; their exact commands remain in `status12-old.resources`
and `status12-merged-recompute0.resources`.

An always-indexed scanner and a flat continuation tuple increased cost on small
real uwvm records and were discarded. An initial generic runtime emitter was
also discarded after a 2–3× slowdown; measurements below concern the final compact
implementation with ordinary-emitter projections and region-boundary suffixes.

## Huge uwvm2 compile

| Main translation unit | Result | Seconds | Peak RSS KiB | Peak RSS GiB |
|---|---|---:|---:|---:|
| Before change, 32 GiB virtual ceiling | OOM | 204.01 until failure | 33159056 | 31.62 |
| Final candidate + status-only proof, 12 GiB virtual ceiling | Object complete | 66.60 | 3600412 | 3.43 |

The failed baseline is artifact `compile.WOFswx`; the final candidate is
`compile.SBt7a4`. Both retain the huge-CPO macro and the same optimization flags.
The same status header was force-included for the baseline, whose library does
not consume the new proof. A completed baseline compile duration is unavailable,
so no full-TU compile speedup ratio is inferred from its OOM time.

The final main object has 4,468,052 text bytes, 36,024 data bytes, and 11,661 BSS
bytes. SHA-256:
`d97ffc828367fca047219f18192663afa5471a63b36104a734079334ebe43968`.
An earlier accepted syntax-only probe completed in 18.63 s with 2,497,560 KiB RSS;
it predates the final emitter refinements and is not the final object measurement.

The final actual-uwvm formatter contract was compiled again against
`final-tail/include`. All 256 complete records match the baseline byte and
primitive-operation transcript, SHA-256
`5811999ded96226fdc4d36e91ca796f6f1ff03fa2c6483c0e941e7de80ab7b33`.
The broader policy fixture emits 4,624 transcript records (4,640 print calls,
including 672 exception cases); the wide fixture adds 448 runtime cases and four
constexpr assertions.

The same final headers were then used to build and link the complete interpreter:

| Additional step | Seconds | Peak RSS KiB |
|---|---:|---:|
| `host_api.default.cpp` | 8.91 | 930220 |
| `uwvm_runtime.default.cpp` | 32.55 | 2501688 |
| Link | 0.10 | 100908 |

Artifacts: `compile.SBt7a4/program.qkx9hs2d`. `--version` and `--help` passed.
All eight integration cases passed: color enabled/disabled, each with a valid
`_start` importing the provider and checking `41 + 1 == 42`, plus three exact
function-type mismatch diagnostics. Color-enabled diagnostics contain ANSI SGR;
color-disabled output does not. The negative oracle requires the complete
expected/got signature diagnostic before program execution.

The snapshot's original `consumer_ok` fixture exports `call_f` without an entry
point. It is not counted as either a successful execution or an optimization
regression. The independent positive fixture supplies `_start`; the provider
and three negative modules have bytes identical to the original WABT-generated
fixtures. The runner saves readable WAT, binary Wasm, commands, logs, hashes,
resource reports, and `results.json`.

## Runtime comparison with before-change and official fast_io

`uwvm_condition_bench.cc` is compiled verbatim against all three include roots;
no API adapter is needed for this fixture. It uses eight optional ANSI literals,
mandatory field labels, the actual pinned uwvm memory formatter, and optionally
an actual `iso8601_timestamp`. Both null and valid memory views are tested.
Timestamp creation is outside the timed loop. A synchronous 4096-byte memory
sink measures formatting and dispatch without syscall or terminal noise.

Profiles `00` and `01` use independent predicates without/with a timestamp.
Profile `11` uses the same predicate expression for all eight conditions and
includes a timestamp, modeling uwvm's shared color setting. Each profile runs
all-off, all-on, changing correlated, and changing independent masks. No
status-proof integration header is used for these small benchmark records.

The driver freshly compiles every version/profile with
`-std=c++26 -O3 -march=native` on CPU 14, saves compiler resource reports and text
sizes, and rotates version order across three rounds of 3,000,000 records per
run. Full-byte fixtures separately cover all 256 masks and both memory views.
The timed loop additionally checks length checksums and sampled materialized
bytes. Before/after fast_io_new must also retain the same primitive-call counts.

Official fast_io has a different pre-existing strategy for semantic condition
boundaries, so its operation counts are recorded rather than required to match
fast_io_new. For example, the independent timestamp full-byte fixture uses
11,520 primitive calls in official fast_io and 5,888 in fast_io_new while
producing identical output. This is a comparison of equal formatter workloads,
not a claim of identical cross-version CPO policy or whole-interpreter throughput.

Reproduce with the three isolated library trees:

```sh
python3 benchmark/0026.cpo_matrix/run_uwvm_condition_bench.py \
  /path/to/pinned/uwvm-snapshot /path/to/before/include \
  /path/to/after/include /path/to/official/include /path/to/disk-artifacts \
  --cxx "$CXX" --cpu 14 --rounds 3 --iterations 3000000
```

The accepted fresh three-way run is
`threeway/condition-bench-__zbsxq_`. All 72 comparison groups (216 timed runs)
passed. Each of the three profiles also passed 512 complete-byte comparisons
across all three libraries. See the
[complete compile and runtime table](BENCHMARK_UWVM_CONDITIONS_20260905.md).

For the shared-color-plus-timestamp profile, the small benchmark executable
build measured:

| Library | Build seconds | Peak RSS MiB | Text bytes |
|---|---:|---:|---:|
| Before-change fast_io_new | 8.81 | 1035.79 | 282805 |
| Final fast_io_new | 4.47 | 568.13 | 16392 |
| Official fast_io | 2.53 | 557.61 | 12453 |

The final version reduces this benchmark's build time by 49.3% and peak RSS by
45.2% versus before-change fast_io_new. Official fast_io still builds this small
fixture faster and uses slightly less compile memory. These executable-build
numbers include linking and must not be confused with the huge main-TU table.

Representative runtime medians, in ns per record (lower is faster):

| Workload | Before | Final | Official |
|---|---:|---:|---:|
| Shared color off, timestamp, valid memory | 43.248 | 38.193 | 40.667 |
| Shared color on, timestamp, valid memory | 54.955 | 49.580 | 50.418 |
| Independent random colors, timestamp, valid memory | 102.067 | 82.185 | 66.104 |
| Independent random colors, no timestamp, null memory | 68.342 | 62.362 | 36.087 |

All 24 measured final medians improve over before-change fast_io_new, with
8.8%–29.9% less time per record. The eight shared-predicate/timestamp combinations
also improve over official fast_io by 1.7%–16.4% in this run. Independent random
conditions favor official fast_io: the largest final/official time increase is
72.8%. The change does not make fast_io_new universally faster than official
fast_io, and the measurements do not establish a universal runtime guarantee.
In particular, differences near 1%–2% should not be treated as statistically
established wins from three samples.

Every accepted binary was rebuilt from the same frozen source; the table excludes
superseded runtime prototypes and incomplete measurement attempts.
