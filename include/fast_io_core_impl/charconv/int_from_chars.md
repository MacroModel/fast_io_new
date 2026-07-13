# Integer `fast_io::from_chars` validation and performance on Apple M4

This report was generated on 2026-07-13 from fast_io commit
`ec7634ccf53d2215fc20a50a519fa99b5e7ed895`. Native timings apply only to the
Apple M4 system described below. The Cortex-X4 results are static scheduling
estimates, not native measurements.

## Environment

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

The x86-64 path was audited separately with Apple Clang 21, LLVM 23 llvm-mca,
Compiler Explorer Clang trunk, and Rosetta execution on the M4 host. The retained
implementation has three bounded components:

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

An early generalized SSE implementation incorrectly used the decimal upper
bound for bases below ten. The x86 libFuzzer target reduced this to base 5 input
`11110555555!1111`; the parser incorrectly consumed three `'5'` characters.
The retained code uses the compile-time upper bound `'0' + base`, and the
reproducer now agrees with libc++ at pointer 5 and value 780.

The corrected public `uint64_t` target initially completed 34,135,047
executions in 61 seconds under x86-64 ASan, UBSan, libFuzzer, and Rosetta with
no sanitizer finding or crash artifact. After the final terminated-input
adjustment, a newly compiled target completed another 19,101,889 executions in
61 seconds with no sanitizer finding and no artifact. Deterministic exact and
terminated tests also pass for both baseline x86-64 and Haswell builds. A
constant-evaluation static assertion covers the SWAR fallback. Clang
compilation passes for Core 2, Nehalem, Sandy Bridge, Haswell, Skylake, Alder
Lake, Sapphire Rapids, and Zen 1 through Zen 4. The complete Apple M4 assembly
remains byte-for-byte identical to the pre-x86-change file. The GCC/Clang
builtin path is excluded for pure MSVC builds.

The final corrected Rosetta matrix contains all 665 unsigned `(base, digits)`
points. Rosetta is a translation environment rather than a native x86
performance authority, so the point estimates below are reported without
generalizing them to physical Intel or AMD processors.

| Range | Current/baseline | std/API | Wins vs std | fast_float/API | Wins vs fast_float |
|:---|---:|---:|---:|---:|---:|
| Exact | 0.976x | 1.648x | 628/665 | 1.191x | 564/665 |
| Terminated | 0.960x | 1.755x | 640/665 | 1.169x | 537/665 |

Within the four-digit SWAR window, exact input is 1.292x faster than libc++ and
1.373x faster than fast_float; fast_io wins all 36 points against fast_float.
Skipping the predictably failing SIMD attempt improves all 26 affected
seven-digit terminated points; their public-API geometric-mean time decreases
by 21.0% relative to the previously retained binary in the same Rosetta
environment.
The complete Rosetta matrix does not demonstrate a 30% aggregate advantage over
fast_float: the final measured aggregate advantages are 19.1% for exact input
and 16.9% for terminated input. A native x86-64 run is required before making the
requested 30% production claim.

llvm-mca models the valid four-digit SWAR block at 2.7 to 4.0 cycles and the
valid eight-digit SSE block at 5.5 to 11.3 cycles across Haswell, Skylake,
Alder Lake, Sapphire Rapids, Zen 2, and Zen 4. Haswell and Zen 2 pay the largest
constant-load and port pressure; Skylake-class Intel cores and Zen 4 have the
best modeled SIMD throughput. Compiler Explorer Clang trunk independently
compiles the submitted kernels successfully with `-O3 -std=c++20
-march=haswell`; the response contains direct `vpmaddubsw` and `vpmaddwd`
instructions and no helper call.

## Reproduction

The benchmark binaries were built with the following shape:

```sh
SDK=$(xcrun --show-sdk-path)
/usr/bin/clang++ -std=c++20 -O3 -DNDEBUG -mcpu=apple-m4 \
  -isysroot "$SDK" -Iinclude -I/path/to/fast_float/include \
  wrappers.cc bench.cc -o int_from_chars_bench
```

Each production fuzz target used:

```sh
clang++ -std=c++20 -O1 -g -fno-omit-frame-pointer -fuse-ld=lld \
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
- Retained x86 exact CSV: `a3e4d16eeb56ee936bf3c1f5d285e28a292eb62af7fd88aa7fd6cd22b16d17e1`.
- Retained x86 terminated CSV: `e6121cf6a1c883883e3b294262033778d7171d4cac7a9e34ad942675586b96fe`.
- x86 final-source fuzz log: `baf8a8d357ddcee0ae05bbbc9d5131c9a531ea576d1ca4177c5c5d4f97a6c1a7`.
- Compiler Explorer Clang-trunk response: `8553a06885e5fb137bc773654f5078acf0aa318b84f5c1a77cbbe8ece63d51ba`.

The corresponding sources, binaries, corpora, logs, assembly, llvm-mca output,
and CSV files are retained under `/tmp/fast_io_production_fuzz` and
`/tmp/fast_io_fullbench` for local inspection.
