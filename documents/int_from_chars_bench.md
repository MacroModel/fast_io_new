# Integer `fast_io::from_chars` validation on Apple M4 and Intel x86-64

This report was generated on 2026-07-13 from fast_io commit
`f5839b8e44ab58ce8fbddf6fc8a0ca69d32b6082`. Native measurements are reported
separately for Apple M4 and Intel Core i9-14900HX. The Cortex-X4 results are
static scheduling estimates, not native measurements.

> This body preserves dated evidence from earlier source snapshots.  The
> current production conclusions are recorded in the
> [2026-07-17 production addendum](#production-addendum-frozen-integer-input-snapshot-2026-07-17),
> which supersedes unqualified uses of "final" below.

## Apple M4 environment

- Processor: Apple M4, 10 logical CPUs, 16 GiB memory.
- Operating system: macOS 26.5.1 (25F80).
- Benchmark compiler: Apple Clang 21.0.0 (`clang-2100.0.123.102`).
- Benchmark target and options: native arm64, C++20, `-O3 -DNDEBUG
  -mcpu=apple-m4`.
- Standard-library reference: the libc++ supplied with Apple Clang 21.
- fast_float reference: commit
  `f8c573d7419ab08de382c60005e37caa271e869a`.
- Fuzzing compiler: Clang 23.0.0git, LLVM commit
  `004ffb73ee4c9b04407eae7c581a872ee328cc84`.
- Fuzzing instrumentation: `-O1 -g -fno-omit-frame-pointer
  -fsanitize=fuzzer,address,undefined`, linked with the matching LLVM 23
  `ld64.lld` and compiler-rt.

## Production fuzzing

Nine independent native-arm64 libFuzzer targets ran for one minute each.
libFuzzer reported 61 elapsed seconds for each target because its time limit is
checked between executions. Standard output, standard error, sanitizer reports,
and crash artifacts were redirected to `/tmp/fast_io_production_fuzz`; no raw
fuzzer stream was emitted to the controlling console.

Every target started with 1,785 deterministic corpus entries. The seed matrix
contains every base from 2 through 36 and every length from one digit through
the maximum `uint64_t` length for that base. It also includes zero, signed and
unsigned limits, negative values, exact ranges, a trailing non-digit, empty
input, sign-only input, leading plus, invalid leading characters, and overflow
strings. Mutation then explores arbitrary bytes and lengths up to 260 bytes.

The public-API targets compare value, error code, and returned pointer directly
with `std::from_chars`. The `char` core target independently maps the internal
parse result to the standard contract and compares it with libc++. The wide
character targets compare their signed and unsigned core instantiations with
the already validated `char` core. Packed modes construct non-ASCII 16-bit and
32-bit code units so that wide-character rejection paths are exercised rather
than testing widened ASCII alone.

| Target | Duration | Executions | Peak RSS | Result |
|:---|---:|---:|---:|:---|
| Public API, `uint64_t` | 61 s | 48,192,806 | 603 MiB | pass |
| Public API, `int64_t` | 61 s | 50,756,214 | 563 MiB | pass |
| Core, `char` | 61 s | 33,762,638 | 844 MiB | pass |
| Core, `signed char` | 61 s | 30,835,739 | 559 MiB | pass |
| Core, `unsigned char` | 61 s | 31,353,695 | 557 MiB | pass |
| Core, `wchar_t` | 61 s | 29,241,177 | 904 MiB | pass |
| Core, `char8_t` | 61 s | 31,006,946 | 571 MiB | pass |
| Core, `char16_t` | 61 s | 30,125,219 | 889 MiB | pass |
| Core, `char32_t` | 61 s | 28,364,592 | 886 MiB | pass |
| **Total** | **549 s** | **313,639,026** | — | **pass** |

No AddressSanitizer or UndefinedBehaviorSanitizer finding occurred, and no
crash artifact was produced. The final pass therefore required no correctness
change.

## Benchmark design

The benchmark covers all valid digit lengths for every base from 2 through 36:

- `uint64_t`: 665 `(base, digits)` points.
- Non-negative `int64_t`: 656 points.
- Negative `int64_t`: the same 656 magnitude-length points, with an additional
  leading minus sign in the parsed range.

Two range shapes are measured independently. The exact shape sets `last` to the
end of the digits. The terminated shape includes one trailing `'#'`, requiring
the parser to locate and return the first non-digit.

Each point contains 2,048 deterministic values drawn from the exact digit-length
interval. A sample performs 128 passes over those values. The public fast_io
API, fast_io integer core, `std::from_chars`, and `fast_float::from_chars` are
rotated within each round; nine samples are collected and the median is
reported. Summary ratios are geometric means across all points. A speedup above
one favors the public fast_io API. The API/core ratio is below one only when a
public-API specialization is faster than the generic core path.

The harness uses identical noinline function-pointer boundaries for all four
implementations. Consequently, the absolute nanosecond values include a common
out-of-line call cost. This is appropriate for comparative dispatch testing but
does not represent the best possible fully inlined call site. The results are
single-process medians; they are not confidence intervals.

## Complete M4 results

| Value set | Range | Points | API/core | std/API | API wins vs std | fast_float/API | API wins vs fast_float |
|:---|:---|---:|---:|---:|---:|---:|---:|
| `uint64_t` | exact | 665 | 1.023x | 2.008x | 659/665 | 1.342x | 628/665 |
| `uint64_t` | terminated | 665 | 1.004x | 1.988x | 665/665 | 1.464x | 651/665 |
| non-negative `int64_t` | exact | 656 | 1.024x | 2.001x | 650/656 | 1.277x | 610/656 |
| non-negative `int64_t` | terminated | 656 | 1.010x | 1.925x | 656/656 | 1.372x | 645/656 |
| negative `int64_t` | exact | 656 | 1.026x | 1.949x | 648/656 | 1.279x | 605/656 |
| negative `int64_t` | terminated | 656 | 1.006x | 1.875x | 655/656 | 1.376x | 643/656 |

The terminated unsigned matrix is faster than `std::from_chars` at all 665
points. Both terminated signed matrices are also nearly complete pointwise
wins. Exact-range misses are concentrated in very short base-2/base-3 inputs
and selected base-7/base-8 signed lengths. They are retained in the table rather
than being hidden by the favorable geometric mean. This run does not support a
claim that fast_io wins at every single input length.

### Unsigned results by base family

| Bases | Points | Range | API/core | std/API | fast_float/API |
|:---|---:|:---|---:|---:|---:|
| 2 | 64 | exact | 1.069x | 2.205x | 1.837x |
| 3–9 | 192 | exact | 1.043x | 1.496x | 1.562x |
| 10 | 20 | exact | 1.087x | 2.683x | 1.429x |
| 11–15 | 89 | exact | 0.992x | 2.167x | 1.117x |
| 16 | 16 | exact | 0.998x | 2.414x | 1.197x |
| 17–36 | 284 | exact | 1.008x | 2.271x | 1.197x |
| 2 | 64 | terminated | 1.066x | 2.231x | 1.993x |
| 3–9 | 192 | terminated | 1.025x | 1.564x | 1.741x |
| 10 | 20 | terminated | 1.041x | 2.669x | 1.654x |
| 11–15 | 89 | terminated | 0.939x | 2.235x | 1.255x |
| 16 | 16 | terminated | 0.970x | 2.393x | 1.222x |
| 17–36 | 284 | terminated | 0.996x | 2.128x | 1.277x |

Decimal is the primary optimization target. Across all 20 unsigned decimal
lengths, the public API is 2.683x faster than libc++ for exact ranges and 2.669x
faster for terminated ranges. It wins all 20 points against libc++ in both
range shapes. The remaining API/core difference reflects the public wrapper,
runtime-base switch, and the fact that the M4 path deliberately does not use the
x86-only one-digit shortcut.

## AArch64 isolation and assembly audit

The pulled one-digit decimal shortcut and its mandatory empty-range guard are
inside a single x86-64 conditional in `int_from_chars.h`. ARM64EC is explicitly
excluded. This is important because leaving only the empty-range test visible
to AArch64 changed Clang's control-flow layout even though the decimal shortcut
itself was disabled.

The complete M4 assembly generated for the unsigned public and core wrappers at
bases 2 through 36 is byte-for-byte identical to the saved pre-Linux-optimization
baseline at commit `2290769197d3692e86db7428a4c66b815fc1e4f5`: both files contain
34,616 lines. The x86-64 preprocessed body is unchanged, so the pulled x86
optimization is retained.

The signed M4 comparison contains ten assembly hunks, covering the public and
core instances for bases 11 through 15. Every hunk changes one `cmp` immediate
from 8 to that base's safe non-overflowing digit limit (16 through 18). No
instruction, branch, load, store, or uOp is added. Native measurement confirms
that long signed inputs in these bases improve: exact public-API time decreases
by 6.56% and terminated public-API time decreases by 6.40% relative to the
pre-change M4 baseline.

### Apple M-series versus traditional AArch64 follow-up

The input path was audited again with a stricter platform-split rule: an
optimization may be guarded by `__APPLE__` and AArch64 only when native M4
improves and the traditional AArch64 path does not. If both processor families
benefit, the optimization remains shared AArch64 code.  At the time of this
follow-up, the implementation contained no Apple-only input branch.  The later
2026-07-17 snapshot deliberately adds one isolated Apple-AArch64 base-2 mask;
see the production addendum.  The decimal AdvSIMD helper, nine-digit inline
limit, 20-digit scalar SWAR path, and first-digit accumulator remain shared
AArch64 code.

The following table is the static evidence recorded for that historical
snapshot.  The current frozen-source values are reported in the production
addendum.  The NEON decision uses both native M4 timing and llvm-mca. On M4, the existing
16-digit NEON kernel measured 1.31--1.43 ns, while the scalar SWAR alternative
measured 1.73--1.94 ns. Static block-throughput estimates also favor NEON on
every tested model; lower is better:

| Model | NEON | Scalar SWAR |
|:---|---:|---:|
| Apple M1--M4 | 5.5 | 12.0 |
| Cortex-A53 | 18.0 | 21.0 |
| Cortex-A76 | 11.3 | 14.0 |
| Cortex-X1 | 4.3 | 7.0 |
| Cortex-X4 | 3.7 | 4.5 |
| Neoverse N1 | 12.7 | 21.0 |
| Neoverse N2 | 8.5 | 8.8 |
| Neoverse V2 | 6.2 | 7.2 |

The fresh 2026-07-17 regions report `5.7/12.0` cycles for M4,
`11.7/14.0` for Cortex-A76, and `8.5/8.8` for Neoverse-N2.  The N2 result is
near parity, not evidence of a large static win.

The generic nine-digit inline and first-digit-accumulator paths were checked by
temporarily removing them from non-Apple AArch64. The Cortex-X4 assembly then
reintroduced an out-of-line runtime scanner call for the base-2 nine-digit
case, additional stack traffic, and an extra table load and loop iteration for
non-overflowing bases above 16. Those changes are regressions on traditional
AArch64, so the optimizations are intentionally not Apple-isolated.

Four Apple-only candidates were measured and rejected. Each ratio below is
baseline time divided by candidate time; values below one are regressions.

| Candidate and measured range | Public API | Internal core | Decision |
|:---|---:|---:|:---|
| Arithmetic ASCII mapping, bases 17--36, all lengths | 0.892x | 0.767x | reject |
| Exact one-digit early return, bases 17--36 only | 1.368x | 1.139x | local win |
| Same early return, bases 17--36, complete matrix | 0.912x | 1.020x | reject |
| Same early return, bases 5--9, complete matrix | 0.940x | 0.992x | reject |
| SWAR invalid-block branch-layout change, bases 2--16 | 0.963x | 0.994x | reject |
| Paired short-decimal unroll | 0.968x | 0.999x | reject |

Moving the 16-digit NEON branch earlier improved long decimal inputs but
repeatably regressed short internal-core inputs by 2.0--2.2%, so it was also
rejected. A replacement NEON unzip/widen reduction was 0.3--0.8% slower than
the retained kernel. No candidate was retained merely because one length or
the public wrapper improved.

After reverting the rejected candidates, fixed public and core wrappers for
bases 2, 7, 8, 10, 16, 17, and 36 were compiled independently for Apple M4,
Cortex-A53, Cortex-A76, Cortex-X1, Cortex-X4, Neoverse N1, Neoverse N2, and
Neoverse V2. For every target the complete generated assembly is byte-for-byte
identical to the pre-experiment baseline. The final 1,330-point native M4
unsigned matrix (all valid lengths, bases 2--36, exact and terminated) also
performed a correctness preflight against `std::from_chars` before timing.
Across that follow-up matrix, fast_io was 2.463x faster than libc++ and 1.445x
faster than `fast_float::from_chars` by geometric mean.

## llvm-mca scheduling model

The full parser contains mutually exclusive length and error paths. Feeding the
entire function to llvm-mca would incorrectly execute all of those paths in one
linear iteration and would assign synthetic latency to calls. Instead, the
following numbers use explicit regions copied from the generated unsigned
base-10 hot path. The entry region contains the range check, first load, ASCII
normalization, and digit validation. The digit region contains one unrolled
pointer check, load, digit validation, multiply-by-ten, and accumulation step.

| Region | Model | Instructions | uOps | Dispatch width | Block throughput |
|:---|:---|---:|---:|---:|---:|
| Entry validation | Apple M4 | 6 | 6 | 6 | 1.5 cycles |
| Decimal digit | Apple M4 | 10 | 10 | 6 | 2.8 cycles |
| Entry validation | Cortex-X4 | 6 | 6 | 10 | 0.7 cycles |
| Decimal digit | Cortex-X4 | 10 | 10 | 10 | 1.0 cycles |

These are steady-state static scheduling estimates. They do not include branch
prediction, cache misses, frequency behavior, or the mutually exclusive exit
paths, and must not be read as native Cortex-X4 benchmark results.

## x86-64 Clang optimization pass

The historical measurements in this section were collected natively on an Intel Core
i9-14900HX under Ubuntu, kernel 6.17.0-29-generic. The benchmark was compiled
with Ubuntu Clang 21.1.8, C++20, `-O3 -DNDEBUG -march=native`, libstdc++ 15.2,
and the same fast_float revision used by the M4 benchmark. The process was
pinned to logical CPU 4, a 5.8 GHz P-core thread, so it could not migrate
between P-cores and E-cores. Rosetta measurements were used only to screen
early candidates and are not used for the final performance claims.

The optimized kernels now live in `integers/sto/sto_contiguous.h`, below the
public character-conversion wrapper. Consequently both internal fast_io
integer scanning and `fast_io::from_chars` use the same implementation. The
x86-64 public wrapper performs only the runtime-base dispatch, standard error
mapping, and result construction; the duplicated x86 fast paths were removed.
AArch64 retains its existing public specializations. The shared x86
implementation has four bounded components:

- Bases 2 through 10 use a 32-bit SWAR kernel for four-digit prefixes. The
  kernel performs one safe four-byte load, parallel digit validation, and two
  scalar multiply-add reductions. It is used only for four- through seven-byte
  ranges, where overflow is impossible.
- Base 8 composes two validated four-digit SWAR blocks when at least eight
  digits are available.
- SSE4.1 builds use one safe eight-byte load for selected bases whose complete
  matrices benefited. ASCII digit and letter validation is performed in
  parallel, followed by `__builtin_ia32_pmaddubsw128` and
  `__builtin_ia32_pmaddwd128`. The implementation does not include
  `<immintrin.h>` and does not over-read the input range. An eight-byte range
  ending in a non-digit bypasses SIMD, avoiding a failed eight-byte validation
  followed by duplicate scalar parsing for seven-digit terminated input.
- Decimal one-digit parsing and the existing short-input paths for bases 3, 4,
  and 11 through 16 are performed inside the internal scanner. This avoids
  giving the public wrapper a faster private implementation than internal I/O.

An early generalized SSE implementation incorrectly used the decimal upper
bound for bases below ten. The x86 libFuzzer target reduced this to base 5 input
`11110555555!1111`; the parser incorrectly consumed three `'5'` characters.
The retained code uses the compile-time upper bound `'0' + base`, and the
reproducer now agrees with libc++ at pointer 5 and value 780.

The native matrix contains all 665 unsigned `(base, digits)` points for both
exact and terminated ranges. Ratios are geometric means; a competitor/parser
ratio above one favors fast_io. `core/API` below one means that internal
scanning is faster after removing the public wrapper.

| Range | core/API | Core wins vs API | std/API | API wins vs std | fast_float/API | API wins vs fast_float | std/core | Core wins vs std | fast_float/core | Core wins vs fast_float |
|:---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Exact | 0.919x | 526/665 | 1.304x | 515/665 | 1.352x | 553/665 | 1.418x | 584/665 | 1.470x | 603/665 |
| Terminated | 0.943x | 481/665 | 1.308x | 538/665 | 1.406x | 614/665 | 1.386x | 577/665 | 1.490x | 620/665 |

The public API therefore exceeds both comparison implementations by more than
1.30x in both range shapes. Internal scanning is faster still: 8.1% faster
than the API for exact input and 5.7% faster for terminated input. Every base
family has a core/API geometric mean below one except terminated decimal at
1.004x, a 0.4% difference within the observed run-to-run noise.

Nine independent native Linux libFuzzer targets were compiled with Clang 21,
ASan, and UBSan and ran for 61 seconds each. The two public targets compare
directly with `std::from_chars`; every core target checks both signed and
unsigned results, returned pointers, and parse codes. No sanitizer finding or
artifact was produced.

| Target | Executions | Peak RSS | Result |
|:---|---:|---:|:---|
| Public API, `uint64_t` | 25,003,675 | 435 MiB | pass |
| Public API, `int64_t` | 28,759,242 | 431 MiB | pass |
| Core, `char` | 21,871,477 | 429 MiB | pass |
| Core, `wchar_t` | 18,850,307 | 394 MiB | pass |
| Core, `char8_t` | 18,851,211 | 435 MiB | pass |
| Core, `char16_t` | 19,250,178 | 398 MiB | pass |
| Core, `char32_t` | 18,502,510 | 395 MiB | pass |
| Core, `signed char` | 18,384,103 | 436 MiB | pass |
| Core, `unsigned char` | 10,811,814 | 427 MiB | pass |
| **Total** | **180,284,517** | — | **pass** |

Deterministic exact and terminated tests pass for baseline SSE2 and Haswell
builds, and a constant-evaluation static assertion covers the runtime fallback.
Clang 21 compilation passes for Core 2, Nehalem, Sandy Bridge, Haswell,
Skylake, Alder Lake, Sapphire Rapids, and Zen 1 through Zen 4. At this SSE
follow-up stage, the complete 665-wrapper Apple M4 assembly remained
byte-for-byte identical to the saved pre-x86-change file. Pure MSVC builds do
not enter the GCC/Clang builtin path. The later production follow-up below
records the intentional M4 layout change from removing whole-scanner forced
inlining.

llvm-mca models the valid four-digit SWAR block at 2.7 to 4.0 cycles and the
valid eight-digit SSE block at 5.5 to 11.3 cycles across Haswell, Skylake,
Alder Lake, Sapphire Rapids, Zen 2, and Zen 4. Haswell and Zen 2 pay the largest
constant-load and port pressure; Skylake-class Intel cores and Zen 4 have the
best modeled SIMD throughput. Compiler Explorer Clang trunk independently
compiles the submitted kernels successfully with `-O3 -std=c++20
-march=haswell`; the response contains direct `vpmaddubsw` and `vpmaddwd`
instructions and no helper call.

## GCC 13--16 high-ISA front-end follow-up

This follow-up was performed on 2026-07-15 against commit
`5df0a94736da0f7363c346d334e87d2e0d0d406e`. It addresses a GCC-only
regression discovered by compiling the shared integer scanner for SSE4.1 and
higher targets. The native machine was the same Intel Core i9-14900HX used
above. Every timed process was pinned with `taskset -c 4` to one 5.8 GHz
P-core, builds and runs were serialized, and no E-core result was used.

The GCC matrix used GCC 13.4, 14.3, 15.2, and a GCC 16 development snapshot.
GCC 14 and 16 were run from isolated unpacked toolchain roots. Clang 23.0.0git
was the compiler control. Every executable used C++20, `-O3 -DNDEBUG
-march=haswell`.
The benchmark retained the 2,048 inputs, 128 timed passes, nine rotated samples,
and per-point median described above. Thus every row below covers all 665
unsigned `(base, digits)` points rather than a selected set of lengths.

GCC expanded the generic eight-byte SSE4.1 template much more aggressively
than Clang. In particular, every base specialization materialized several
byte-splat constants, copied a complete SIMD validation and reduction graph,
and retained the scalar retry graph. The result was a substantial instruction
cache and decode footprint even though the isolated SIMD arithmetic block was
competitive. The retained fix therefore disables only this generic eight-byte
entry for GCC proper. The specialized base-8, base-10, and base-16 SIMD kernels
remain enabled, as do the generic SSE kernel on Clang and other compilers.
There is no new function, runtime dispatch, ISA probe, intrinsic header, or
public-wrapper-only fast path.

The paired baseline/candidate ratios below are geometric means. A
baseline/candidate ratio above one is the speedup delivered by the change; the
competitor/API ratios above one favor the final fast_io implementation.

| Compiler | Range | Baseline/candidate API | Baseline/candidate core | std/API | fast_float/API |
|:---|:---|---:|---:|---:|---:|
| GCC 13.4 | exact | 1.312x | 1.309x | 1.430x | 1.163x |
| GCC 13.4 | terminated | 1.419x | 1.471x | 1.503x | 1.090x |
| GCC 14.3 | exact | 1.389x | 1.369x | 1.329x | 1.157x |
| GCC 14.3 | terminated | 1.262x | 1.253x | 1.350x | 1.084x |
| GCC 15.2 | exact | 1.324x | 1.323x | 1.326x | 1.304x |
| GCC 15.2 | terminated | 1.242x | 1.309x | 1.200x | 1.114x |
| GCC 16 | exact | 1.344x | 1.317x | 1.322x | 1.216x |
| GCC 16 | terminated | 1.221x | 1.276x | 1.231x | 1.078x |

The final independent matrix reproduced an aggregate lead over both comparison
implementations for every GCC version and both range shapes. Pointwise misses
remain, especially very short inputs and several decimal lengths; the table is
an all-point geometric mean and is deliberately not presented as a claim that
every individual duration is lower than both competitors.

### Assembly size and front-end pressure

The complete 665-wrapper executable shrank for every tested GCC release:

| Compiler | Baseline `.text` | Final `.text` | Reduction |
|:---|---:|---:|---:|
| GCC 13.4 | 123,019 B | 97,387 B | 20.8% |
| GCC 14.3 | 126,116 B | 98,428 B | 22.0% |
| GCC 15.2 | 154,704 B | 127,168 B | 17.8% |
| GCC 16 | 163,264 B | 132,424 B | 18.9% |

For GCC 15 base 36, the public fixed-base instance fell from 1,253 bytes and
304 instructions to 837 bytes and 213 instructions. The internal-core instance
fell from 805 bytes and 204 instructions to 383 bytes and 115 instructions.
This is the same parser used by internal scanning, so the core improvement is
not hidden behind the public `from_chars` wrapper.

llvm-mca 23 was run on the generated GCC 15 base-36 hot regions. The old region
is one complete eight-digit SIMD setup/validation/reduction block; the new
region is one scalar digit iteration. The final column multiplies the scalar
block throughput by eight only to place the units on the same digit count.

| Scheduling model | Old SIMD, 8 digits | Final scalar, 1 digit | Final scalar x8 |
|:---|---:|---:|---:|
| Haswell | 16.0 cycles | 2.5 cycles | 20.0 cycles |
| Skylake | 14.0 cycles | 1.7 cycles | 13.6 cycles |
| Alder Lake P-core | 14.0 cycles | 2.0 cycles | 16.0 cycles |
| Sapphire Rapids | 14.0 cycles | 2.0 cycles | 16.0 cycles |
| Zen 2 | 13.5 cycles | 2.5 cycles | 20.0 cycles |
| Zen 4 | 10.5 cycles | 2.0 cycles | 16.0 cycles |

These figures explain why the SIMD helper remains useful under Clang and why
the specialized GCC kernels were not removed. They also show why llvm-mca
alone cannot predict this fix: it models a selected steady-state region, not
the 18--22% whole-program code-size reduction, instruction-cache residency,
decode locality, or entry/fallback control-flow layout. Native full-matrix
timing made the GCC decision; llvm-mca was used to prevent an incorrect claim
that the isolated SIMD multiply-add sequence itself was slow.

Three broader changes were measured and rejected. Disabling the specialized
decimal kernel regressed the all-point matrix. Disabling the short hexadecimal
or 16-byte octal/hexadecimal kernels improved aggregate layout slightly but
regressed their target bases by up to approximately 11%. Marking the generic
helper noinline retained the large call-site graph and did not recover
throughput. A GCC `target("sse4.1,no-avx,no-avx2")` experiment was also rejected:
cross-header `always_inline` helpers produce target-option mismatches, and
spreading target attributes into unrelated character tables and containers
would violate the required ISA isolation.

### Compiler and AArch64 isolation

This specific GCC SSE change is guarded by `__SSE4_1__` and GCC-proper
detection. At that stage, the following complete assembly comparisons were
byte-for-byte identical to the unmodified HEAD baseline:

- Apple Clang 21, `-mcpu=apple-m4`: 34,616 lines, SHA-256
  `33df26da6364c6176b0960b0bdccbbb32f73d461570b1e98728101b25c447e12`.
- Clang 23, x86-64 Haswell: SHA-256
  `5fedb7d4da20d8b11b28bea66deff1f9f378461dde07ec5e56fd8d9f4e48095c`.
- GCC 15, SSSE3 with SSE4.1 disabled: SHA-256
  `26848b5c4e951368f6f76a6a6e58e8d0db8747be4dc46fde5721f857af1e786a`.

Because AArch64 never defines `__SSE4_1__`, both Apple M-series and traditional
AArch64 remain in the exact same preprocessed and generated path. A fresh
single-process M4 matrix confirms the assembly result:

| Range | core/API | std/API | API wins vs std | fast_float/API | API wins vs fast_float |
|:---|---:|---:|---:|---:|---:|
| exact | 0.979x | 1.997x | 660/665 | 1.349x | 624/665 |
| terminated | 0.995x | 1.990x | 665/665 | 1.463x | 655/665 |

### GCC-path correctness validation

The GCC-specific branch cannot be exercised by Clang libFuzzer, so it received
an additional differential driver compiled independently with GCC 13, 14, 15,
and 16. Public `uint64_t` and shared-core signed/unsigned targets covered bases
2 through 36, every length from one through the maximum base length, exact and
terminated ranges, upper- and lower-case letters, an invalid character at
every input position, and explicit overflow strings. Each binary then checked
one million deterministic pseudo-random byte strings against
`std::from_chars`. All eight compiler/target combinations passed. The affected
GCC 15 public path also passed the same driver under AddressSanitizer and
UndefinedBehaviorSanitizer.

## Reproduction

The benchmark binaries were built with the following shape:

```sh
# Native Apple M4
SDK=$(xcrun --show-sdk-path)
/usr/bin/clang++ -std=c++20 -O3 -DNDEBUG -mcpu=apple-m4 \
  -isysroot "$SDK" -Iinclude -I/path/to/fast_float/include \
  wrappers.cc bench.cc -o int_from_chars_bench

# Native Intel Core i9-14900HX
clang++-21 -std=c++20 -O3 -DNDEBUG -march=native \
  -Iinclude -I/path/to/fast_float/include \
  wrappers.cc bench.cc -o int_from_chars_bench
taskset -c 4 ./int_from_chars_bench > results.csv
```

Each production fuzz target used:

```sh
clang++-21 -std=c++20 -O1 -g -fno-omit-frame-pointer \
  -fsanitize=fuzzer,address,undefined -DFUZZ_KIND=N -Iinclude \
  fuzz_int_from_chars.cc -o fuzz_target
fuzz_target -max_total_time=60 -timeout=5 -rss_limit_mb=4096 \
  -max_len=260 -print_final_stats=1 -reload=0 \
  -artifact_prefix=/tmp/fast_io_production_fuzz/artifacts/ corpus
```

Primary artifact SHA-256 values:

- Fuzzer harness: `5615b9369131508b624c79e7c2a384e02e6b01f10050f4fb8457d851f32fed22`.
- Corpus generator: `5c042675399637a27cccd142da10c18a1a5521b09f06873d7181a7c4ea9183b7`.
- Unsigned exact CSV: `b2844ae52c07ba302bd8e5e03d4b5b0d5f62d00395004e05d1440b38c8531b77`.
- Unsigned terminated CSV: `3d0db8c34f40988a0b2bd04ebe529ef0e132f7beeca6a826d4af32311c31f7c2`.
- Non-negative signed exact CSV: `76cf51eafcfab29b0ad95c235f07e5b3fca9a2327b812cf773ed98d2de3d37bd`.
- Non-negative signed terminated CSV: `5db9e7fdf2a9bbf190a7654d5b79977d1205b2c087dcdf30691ba40dcfbd4893`.
- Negative signed exact CSV: `4e247062d9da3c6009acd6d1c16de2eeb857d78c6fe6443e6ba38945a638a915`.
- Negative signed terminated CSV: `146f70ab27c02d914432cac2c0dee3c55f316387c3f2683c16ec68afac16bb14`.
- llvm-mca region source: `84960af382516afe7edccf8808504279a087adb95073009d3b44c14e042baab1`.
- Apple/traditional AArch64 follow-up CSV:
  `395bb43a022af4643eeb02a6fbf010d6a2e5aeb4f568d79a25757d15556be389`.
- Decimal-16 follow-up llvm-mca source and summary:
  `eaa1e8f1a6f8df3cb27b549e1da48fc3a9850e118d5b96b4742050a393bc67cb`
  and `d89607560b8000bc4371b5295ee75d8e81507bf42472c0b1e5849cc6760bf13a`.
- Final Apple M4 and Cortex-X4 follow-up assembly:
  `43a98071322ec4e35e92e90641923ebb5697e7d6a698e3c3a1aa64cb5aa01a33`
  and `159d3799151f956379fcdba76598e1c8d08676de5340e092be704bda1a5a8c4a`.
- Native x86 exact CSV: `cb8e271f6fe89ad06e904ef76f803ece956425e9f6a81ea2074a2af9df021299`.
- Native x86 terminated CSV: `57de8a75d736050c68d92fdd4350774ea5127c48f96b43aa32cc31a4235ecf20`.
- Native x86 fuzz logs, targets 0–8 respectively:
  `38ba22d20a2e6be60d3db933bfdba67b074d8d88319f06f5868de0d0ed47e8d4`,
  `8a2cfe151e65002c38ed68bbf85feb899aaf85a3a94019fb6444bbb34a7cf11e`,
  `5372462338193e22154566c12b962901ee0b84b232712d88269f9c717c7b537a`,
  `8cfba6739fe08506345116008a29555e314ba19bcd48d9e974393615aeba68d6`,
  `06f6936c28bfd261fb662fbeaf068f1133dcd509e95fac1e42f619c1dbe6b048`,
  `7fdd0b5a9818a58abdfc3c5e02fae32aaee44258b5f4c5d0b3ba6afb2cacc40a`,
  `671eb4590ebcb903ffe7256397b049152208c318c56e7b80091f7da46f4f4c71`,
  `e6b68cb8367d2a0a9ec234689e8c193a331918e929a73cf0936a47c437ce5b2a`,
  and `e46e4e283e207b49539d0e804b80e61b9824388254151c8f2ca8371fd8a56a84`.
- Compiler Explorer Clang-trunk response: `8553a06885e5fb137bc773654f5078acf0aa318b84f5c1a77cbbe8ece63d51ba`.

The corresponding sources, binaries, corpora, logs, assembly, llvm-mca output,
and CSV files are retained under `/tmp/fast_io_production_fuzz`,
`/tmp/fast_io_fullbench`, and `/tmp/fast_io_native_bench` on the Linux host.

## 2026-07-16 complete integer-type and compiler matrix

This production follow-up extends the earlier `uint64_t` work to unsigned and
signed 8-, 16-, 32-, 64-, and 128-bit integers.  It covers every valid digit
length in every base from 2 through 36 for `char`, `wchar_t`, `char8_t`,
`char16_t`, and `char32_t`.  Exact input ranges and ranges containing one
trailing non-digit are timed independently.  Correctness preflight additionally
uses 0, 1, 2, 3, 7, 8, 15, and 31 trailing code units.

The historical 2026-07-16 x86-64 measurements were collected on the Intel Core i9-14900HX host with
each process pinned to P-core logical CPU 4.  Builds and runs were serialized.
The compiler matrix is GCC 13.4, GCC 14.3, GCC 15.2, a GCC 16 development
snapshot, Clang 22, upstream Clang 23, and the fast_io Clang 23 toolchain.  All
executables use C++20, `-O3`, and `-march=native`.  The complete result contains
115,620 rows per compiler: 77,080 input rows and 38,540 output rows.  The public
standard-compatible API and `std::from_chars` comparison apply to `char`;
wide-character rows exercise the same internal scanner used by fast_io input.

Ratios above one favor fast_io.  The public/core columns quantify wrapper cost;
the competitor/public columns are paired only where both APIs are available.

| Compiler | public/core | runtime/core | std/public | fast_float/public |
|:---|---:|---:|---:|---:|
| GCC 13.4 | 1.023x | 1.210x | 1.372x | 0.968x |
| GCC 14.3 | 1.023x | 1.105x | 1.430x | 0.961x |
| GCC 15.2 | 1.007x | 1.075x | 0.933x | 1.033x |
| GCC 16 development | 1.066x | 1.226x | 1.524x | 1.110x |
| Clang 22 | 1.008x | 1.108x | 1.242x | 1.132x |
| Clang 23 upstream | 0.996x | 1.060x | 1.239x | 1.172x |
| Clang 23 fast_io | 0.999x | 1.061x | 1.238x | 1.179x |

These complete-matrix results do not support a universal 30% claim.  The final
data explicitly retains aggregate losses: GCC 13/14 lose to fast_float, and
the final GCC 15 monolithic matrix loses to the standard-library instances
that GCC re-inlines after the scanner layout shrinks.  Decimal and bases
17--36 for 32- and 64-bit signed values remain limiting families.  Reporting
these groups is important: an aggregate lead must not be presented as a
pointwise guarantee, and a single large translation unit must not be mistaken
for isolated-library code generation.

### Retained x86-64 short high-base kernels

The retained optimization is inside
`integers/sto/sto_contiguous.h`, below both the public wrapper and internal
fast_io scanning entry.  For bases 17--36 it uses bounded, overflow-safe kernels
for 8-, 16-, and 32-bit destinations.  The maximum work is respectively two,
four, and eight digits.  The scanner consumes the already validated first
digit, accumulates in a wider integer, performs one final range check, and
preserves the original overflow pointer by scanning any remaining digits.
All five character types use the kernels when their code units carry ASCII or
Unicode digit values; execution-EBCDIC `char` and `wchar_t` remain on their
native character path.

On the i9-14900HX, the Clang 23 candidate improved the complete `char` matrix by
1.223x/1.265x for `uint8_t`/`int8_t`, 1.082x/1.119x for
`uint16_t`/`int16_t`, and 1.078x/1.091x for `uint32_t`/`int32_t`.
The corresponding base-17--36 improvements are 1.681x/1.643x,
1.198x/1.303x, and 1.214x/1.215x.  Wide-character high-base gains reach
1.80x for 8-bit destinations, 1.58x for 16-bit destinations, and 1.51x for
32-bit destinations.

A second pass removed a duplicate lookup of a known trailing non-digit.  It
improves Clang trailing ranges by 6.8--9.4%, GCC signed-16 and 32-bit ranges by
4.7--6.1% overall, and is deliberately disabled for GCC `uint16_t`: retaining
state there regressed exact ranges by about 10%.  This is a compiler split, not
a microarchitecture dispatch, and it remains in the same scanner function.

Assembly explains both the gain and its limit.  Clang fully expands the bounded
32-bit kernel; representative base-36 core instances are 439--466 bytes, while
the standard and fast_float wrappers are approximately 200 bytes but retain
loop control or staged overflow checks.  GCC keeps the digit loop compact, but
the complete core instance is 698--760 bytes because its generic leading-zero
graph remains present.  Re-enabling the previous generic GCC SSE4.1 graph was
not beneficial.  A Clang four-way-unroll candidate was also rejected: it
regressed the full `uint32_t` and `int32_t` matrices by 3.5% and 3.0%, and the
high-base subsets by 7.9% and 6.6%.

The per-digit llvm-mca regions contain 9 instructions for fast_io and
fast_float and 12 for the early standard-library phase.  Modeled block
throughput is 1.5/1.5/2.0 cycles on Skylake, 3.0/2.0/2.0 on Alder Lake and
Sapphire Rapids, and 1.8/1.8/2.3 on Zen 3 and Zen 4.  Native results overrule
the isolated Alder Lake region: complete expansion wins because it removes the
dynamic loop backedge and length control that the single-region model cannot
represent.

### GCC decimal, signed hexadecimal, and nine-digit mid-base repair

Pointwise analysis of the final GCC 15 and GCC 16 matrices exposed a second
compiler-specific problem.  Unsigned 32-bit, nine-digit inputs in bases 12
through 15 took approximately 6.7--7.2 ns and could lose to both competitors.
The retained GCC-only x86-64 path loads the remaining eight digits
independently, validates them together, and reduces them with a balanced
pair/quad multiplication tree.  It is enabled only for GCC 15 and newer;
Clang, GCC 13/14, EBCDIC, signed integers, and every non-x86 target retain their
previous graph.

Across GCC 15 and GCC 16, all sixteen measured exact/terminated base points now
beat both `std::from_chars` and `fast_float::from_chars`.  Public times are
3.15--3.69 ns.  The smallest measured advantages are 1.010x over the standard
library and 1.016x over fast_float for a terminated GCC 15 base-12 input; the
largest are 1.415x and 1.515x respectively for the corresponding GCC 16 input.
The wide-character base-14 checks also remain wins, reaching 1.25--1.30x over
fast_float for 16- and 32-bit code units.

The arithmetic change shortens a dependency chain rather than increasing
instruction-level parallel work.  GCC 15 emits 34 instructions and 34 uOps for
both the serial and balanced regions.  llvm-mca reports that a single hot
region falls from 45 to 29 cycles on Skylake, Alder Lake, Sapphire Rapids,
Zen 3, and Zen 4.  Steady-state block throughput remains 9 cycles on Skylake
and 8 cycles on the other models because lookup loads and front-end work still
set the throughput bound.  At 100 iterations the modeled totals change from
936 to 922 cycles on Skylake, 834 to 820 on Alder Lake and Sapphire Rapids,
937 to 820 on Zen 3, and 933 to 820 on Zen 4.  The complete public base-14
instance also shrinks from 1,396 to 1,289 bytes; the core instance shrinks by
five bytes.

An SSE4.1 candidate using the existing eight-digit `__builtin_ia32_*` reduction
was rejected after native testing.  It required 9.0--10.6 ns, approximately
three times the scalar-tree time, and achieved only 0.31--0.44x of competitor
throughput.  The SIMD instruction count therefore did not compensate for its
shuffle, widening, and reduction graph on this workload.

Unsigned 32-bit decimal input had a different nine-digit problem.  The generic
GCC path took approximately 3.3--3.5 ns for an exact range and 5.9--6.1 ns when
the ninth digit was followed by a non-digit.  The final GCC 15+ x86-64 kernel
performs one 64-bit load, parallel ASCII validation, and balanced 10/100/10000
SWAR reductions.  It handles both exact nine-digit input and a ninth digit
followed by a non-digit; valid ten-digit values and every other length continue
through the existing overflow-aware path.  The code is guarded from constant
evaluation, and a nine-digit public `from_chars` call is also compiled as a
`static_assert`.

After removal of the historical whole-scanner force-inline attribute, the
final GCC 15 nine-digit public times are 2.634/2.639 ns for exact/terminated
input.  They retain 1.496x/1.543x advantages over the standard library and
1.080x/1.076x advantages over fast_float.  GCC 16 measures 2.698/2.668 ns in
the focused no-force-inline run.  Public, internal-core, and runtime-base
times are now within approximately one percent at this point because GCC emits
one shared scanner body rather than duplicating the complete graph into each
wrapper.  Complete-matrix values in the compiler table were regenerated from
the final source; older force-inline timings must not be mixed with them.

The arithmetic-only llvm-mca regions contain 25 instructions for the serial
nine-digit dependency chain and 18 for SWAR.  Over 100 modeled iterations,
Skylake changes from 812 to 445 cycles and from 8.0 to 3.2 cycles block
throughput.  Alder Lake and Sapphire Rapids change from 1,608 to 620 cycles
and from 16.0 to 6.0 cycles throughput.  Zen 3 and Zen 4 change from 831 to
668 cycles and from 6.8 to 5.3 cycles throughput.  These regions deliberately
exclude the shared validation/exit graph, so they describe the arithmetic
gain rather than whole-parser latency.

With the scanner no longer forcibly duplicated into every caller, GCC 15 emits
a shared base-10 unsigned-32 scanner of 0x4a9 bytes before the final vectorizer
tuning and 0x419 bytes afterwards.  The public and internal benchmark wrappers
are both small call sites, so their measured cost is effectively equal.
Placing the SWAR block after the existing short-input returns remains best;
marking it cold and several earlier placements were rejected because their
short-input layout losses outweighed the local benefit.

### GCC 15 native-ISA front-end repair

The final GCC-15-only x86-64 adjustment disables loop vectorization for the
scanner when AVX is enabled.  GCC's SLP vectorizer remains enabled, explicit
SSE builtins are unchanged, and no target attribute or microarchitecture
dispatch is introduced.  The complete base-10 unsigned-32 scanner falls from
0x4a9 to 0x419 bytes; in the all-type `char` executable, YMM/ZMM references
fall from 2,061 to 1,378 and total text falls by about 28 KiB.  This reduces
front-end and instruction-cache pressure without changing the scalar/SSE
algorithm selected for any base.

Two independent all-type `char` matrices reproduce the result.  Geometric-mean
core/public/runtime times fall from 6.113/6.191/6.624 ns to
6.052/6.094/6.503 ns, improvements of 1.0%, 1.6%, and 1.8%.  GCC 13 regresses
and GCC 16 regresses in core/public mode, so the exception is deliberately
limited to GCC 15 rather than generalized by ISA.  Disabling only SLP also
regresses.  Function target attributes disabling AVX or AVX2 cost about 30%
and were rejected.

The final GCC 15 base-10 unsigned-32 assembly gives the internal and fixed
public benchmark wrappers the same 0xda-byte body.  Each makes its only hot
call at the same relative offset to the same shared scanner symbol; there is no
public-only parsing kernel.  The noinline runtime-base benchmark body is
0x49d bytes because it contains the base-2-through-36 selector, matching the
remaining 1.075x runtime/core cost.

The SSSE3 build still has lower absolute parser time on this host
(5.903/5.975/6.412 ns).  A native build with
`-mprefer-vector-width=128` narrows the final gap to approximately 1.6--2.2%,
but does not reverse it.  This is not evidence for replacing the native
algorithm: the same independently compiled runs move standard-library and
fast_float time by 6--17%, and GCC materially changes their inlining and code
size when the fast_io scanner layout changes.  Consequently ISA claims are
reported with both absolute and competitor-normalized data; the report does
not present cross-binary layout drift as a parser-kernel win.

The signed 64-bit hexadecimal matrix revealed a separate GCC code-generation
failure in the former SSE graph.  Restricting that graph away from signed
destinations reduces the GCC 15 fixed-base core from 2,916 to 2,372 bytes and
from 677 to 572 disassembled instructions, while the unsigned instance remains
unchanged.  Native affected points improve by approximately 2.9--3.3x.

The final targeted differential test ran with GCC 15 and GCC 16 for bases
12--15, all five character types, exact and eight different trailing lengths,
valid nine-digit values, overflow, an invalid digit at every position, wide
code units above 255, and ten-digit overflow.  More than one billion cases
passed.  GCC 15 repeated the complete targeted run under AddressSanitizer and
UndefinedBehaviorSanitizer.  The decimal follow-up adds more than 70 million
checks per compiler over nine-digit input, valid and overflowing ten-digit
input, eight trailing lengths, invalid digits at every position, all five
character types, and ten million arbitrary eight-byte prefixes tested as both
exact and terminated input; GCC 15 repeated it under ASan and UBSan.  Finally,
the complete 682-point `uint32_t` matrix and correctness preflight were rerun
three times for each affected compiler.  The same differential driver passes
with GCC 15 and GCC 16 at the x86-64 SSE2 baseline, confirming that the SWAR
kernel has no AVX or native-ISA dependency.

### Apple M4 and traditional AArch64 preservation

The same 50-file matrix was run natively on Apple M4 with Apple Clang 21 and
the local Clang 23 toolchain.  Paired public results are:

| Compiler | public/core | runtime/core | std/public | fast_float/public |
|:---|---:|---:|---:|---:|
| Apple Clang 21 | 0.993x | 1.107x | 1.656x | 1.279x |
| Clang 23 | 0.997x | 1.108x | 1.705x | 1.314x |

The bounded kernels and compiler exceptions introduced by this 2026-07-16 x86
follow-up are inside the x86-64 branch.  This historical statement does not
cover the later Apple-AArch64 base-2 mask documented in the 2026-07-17
production addendum.  Removing the historical whole-scanner force-inline
attribute intentionally changes M4 layout: wrapper text grows from 47,664 to
48,004 bytes, but no new out-of-line scanner call appears.  Normalized core
instruction sequences remain unchanged for all bases 2--36; public sequences
change for bases 3, 4, 8, 11--18, 28--30, and 36 because Clang makes different
ordinary inlining choices.  Three paired native quick matrices show a combined
public change of about -0.2% and a core change of about -0.35%, both favorable
and within layout noise.  The final no-force-inline M4 wrapper object has
SHA-256
`6d619991f4a0568ca0ba2e3b6b18629e6f07d458a6f19e3473e00af39ef70d9e`.
The final GCC-15 vectorizer exception is x86-only; rebuilding after that change
produces the same M4 object byte for byte.
The shared wide-character table lookup remains enabled on Apple and traditional
AArch64 because llvm-mca improves on every checked model: M4 block throughput
falls from 4.8 to 2.5 cycles, Cortex-A710/A720 from 3.8 to 2.4, Neoverse N1
from 6.3 to 4.0, and Neoverse V2 from 3.2 to 2.0.

### EBCDIC and final correctness validation

GCC 13 through 16 also ran the complete matrix with
`-fexec-charset=IBM1047 -fwide-exec-charset=IBM1047`.  In this mode `char` and
`wchar_t` use execution-EBCDIC values, whereas `char8_t`, `char16_t`, and
`char32_t` retain Unicode code points.  The benchmark's standard-library leg is
disabled because libstdc++ itself forms a negative index while instantiating
its ASCII `<charconv>` table under IBM1047.  fast_io public/core ratios are
1.006x, 1.003x, 1.004x, and 1.008x for GCC 13 through 16 respectively.

This matrix found and fixed an execution-character bug in the SSE constants:
numeric ASCII byte constants are now used instead of execution `char` literals.
The final enhanced driver passed across every compiler matrix and all affected
integer and character types, including the EBCDIC Unicode combinations.  It
checks radix boundaries, signed minima, unsigned maxima, overflow, empty and
sign-only ranges, leading plus, invalid input, wide code units above 255, and
every requested trailing length.  The additional GCC 15/16 mid-base campaign
described above contributes more than one billion targeted cases.  The final
one-minute-per-type production libFuzzer pass was rebuilt from the
no-force-inline source with Clang 23, ASan, and UBSan.  All ten integer types
completed successfully, totaling 112,377,389 executions; every artifact
directory remained empty.

## Public five-character API follow-up

The public overload is now templated on its input code-unit type and directly
supports `char`, `wchar_t`, `char8_t`, `char16_t`, and `char32_t`.  The integer
type remains the first template parameter, preserving explicit calls such as
`from_chars<uint64_t>(...)`.  `basic_from_chars_result<Char>` carries the
correct `Char const *` pointer; its `char` specialization is exactly
`std::from_chars_result`, so the existing narrow API keeps its result type.
No temporary narrow buffer, transcoding pass, allocation, or second parser is
introduced.

The final deterministic matrix contains all 50 integer/code-unit combinations:
signed and unsigned 8-, 16-, 32-, 64-, and 128-bit integers, all bases 2--36,
every valid digit length, signed minima, unsigned maxima, overflow, invalid
input, and 0/1/2/3/7/8/15/31 trailing code units.  Native M4 Clang and x86-64
GCC 15 on P-core 4 both pass 50/50.  GCC 15 also passes the same 50 combinations
with IBM1047 execution and wide-execution character sets.  A constexpr
round-trip is compiled for every public character type.

The public-interface libFuzzer selects the character type, base, packed wide
code units, arbitrary input bytes, and formatting value on every iteration.
Clang 23 with ASan and UBSan ran each of the ten integer types for 60 seconds,
totaling 76,388,264 executions.  All ten runs completed, all configured
artifact directories remained empty, and no sanitizer diagnostic was found.

The complete 665-point `uint64_t` native timing matrix measures fixed-base
public/core ratios for exact and terminated input together.  A ratio above one
is public overhead:

| Host/compiler | `char` | `wchar_t` | `char8_t` | `char16_t` | `char32_t` |
|:---|---:|---:|---:|---:|---:|
| Apple M4, Apple Clang 21 | 1.013x | 0.995x | 1.000x | 1.004x | 0.993x |
| i9-14900HX, GCC 15 | 0.998x | 1.002x | 1.000x | 0.996x | 0.999x |

On the assembly produced for that follow-up snapshot, representative
`char16_t` base-16 public and core
wrappers are both 92 instructions with no call.  Base-10 uses 53 versus 51
instructions and both make the same single call; the two extra instructions
implement the required `parse_code` to `std::errc` result mapping.  On GCC 15,
both wrappers call the same specialized scanner.  The core call site is 0x4f
bytes and the public call site is 0x68 bytes; the 25-byte difference is solely
error-code mapping and has no measurable aggregate hot-success cost above the
noise shown in the table.
