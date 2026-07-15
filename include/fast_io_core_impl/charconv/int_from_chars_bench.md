# Integer `fast_io::from_chars` validation on Apple M4 and Intel x86-64

This report was generated on 2026-07-13 from fast_io commit
`f5839b8e44ab58ce8fbddf6fc8a0ca69d32b6082`. Native measurements are reported
separately for Apple M4 and Intel Core i9-14900HX. The Cortex-X4 results are
static scheduling estimates, not native measurements.

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
benefit, the optimization remains shared AArch64 code. The final integer input
implementation contains no Apple-only branch. The 16-digit decimal NEON
reduction, nine-digit inline limit, 20-digit scalar SWAR path, and first-digit
accumulator therefore remain available to both Apple and traditional AArch64.

The NEON decision uses both native M4 timing and llvm-mca. On M4, the existing
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

The final x86-64 measurements were collected natively on an Intel Core
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
Skylake, Alder Lake, Sapphire Rapids, and Zen 1 through Zen 4. The complete
665-wrapper Apple M4 assembly remains byte-for-byte identical to the saved
pre-x86-change file. Pure MSVC builds do not enter the GCC/Clang builtin path.

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

The change is guarded by `__SSE4_1__` and GCC-proper detection. The following
complete assembly comparisons are byte-for-byte identical to the unmodified
HEAD baseline:

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
