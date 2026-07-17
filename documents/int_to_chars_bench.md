# Integer `fast_io::to_chars` correctness and performance on Apple M4

This report was generated on 2026-07-12 and updated with the Apple/traditional AArch64 split on 2026-07-13. Native timings apply only to this Apple M4 system; cross-target compilation and static scheduling analysis cannot replace native measurements on other processors.

> This body preserves dated evidence from earlier source snapshots.  The
> current production conclusions are recorded in the
> [2026-07-17 production addendum](#production-addendum-frozen-integer-output-snapshot-2026-07-17),
> which supersedes unqualified uses of "final" below.

## Environment

- System: Apple M4, macOS 26.5.1 (25F80).
- Compiler: Clang 23.0.0git, LLVM commit `004ffb73ee4c9b04407eae7c581a872ee328cc84`.
- Target: `aarch64-apple-darwin25.5.0`.
- Release options: `-O3 --sysroot=$SYSROOT -fuse-ld=lld -std=c++20 -march=native`.
- fmt: 12.2.0, linked from `src/format.cc`; the benchmark introduces no configuration macro.
- Timer: `CLOCK_UPTIME_RAW`.

## Correctness

The independent correctness harness covers bases 2 through 36 and fifteen standard integer types: `char`, signed and unsigned character types, `wchar_t`, `char8_t`, `char16_t`, `char32_t`, and all standard signed and unsigned integer widths.

It checks complete 8-bit and 16-bit domains, radix-power boundaries, minimum and maximum values, fixed-seed random values, exact-capacity buffers, every insufficient capacity, returned pointers, `std::errc::value_too_large`, and fast_io's stronger tested invariant that a too-small destination range remains unchanged. Clang Release, Apple Clang ASan+UBSan, and GCC 15 produced the same result:

- 20,939,175 values passed.
- 226,012 capacity and error-contract cases passed.
- The three final logs have the same SHA-256: `5bd13e07773d965460c32b7ee465058d3cf03f09a4447ac7ce8e14b059493b51`.

The platform-split validation adds exhaustive coverage of all 1,835,008 seven-digit octal `uint64_t` values for each of `char`, `wchar_t`, `char8_t`, `char16_t`, and `char32_t`, plus 200,000 random values per character type and radix-power boundaries. It also compares 1,750,000 public base-2-through-base-36 conversions with `std::to_chars`, including standard insufficient-capacity results and the same stronger fast_io range-preservation invariant. Native M4 Release, native M4 ASan+UBSan, and Rosetta x86-64 Release all pass.

The performance harness also performs a byte-for-byte preflight against `std::to_chars` for every timed value and against fmt wherever fmt exposes the comparable core path.

## Benchmark design

- The matrix contains every valid `(base, digits)` point for `uint64_t`: 665 points across bases 2 through 36.
- Each point uses 4,096 deterministic values from its exact digit-length interval, including both boundaries and fixed interior values.
- fast_io and the standard library use their public runtime-base `to_chars` APIs. fmt uses its core integer writer for bases 2, 8, 10, and 16.
- Conversion bodies are inlined into separate measurement loops. No artificial per-value indirect or out-of-line call is added.
- Stores rotate through 256 aligned 128-byte buffers. The checksum consumes the output length and boundary bytes to prevent dead-store elimination without repeatedly targeting one cache line.
- A common repetition count is calibrated per comparison so the slower implementation runs for approximately 0.75 ms per leg.
- Every trial is paired as ABBA or BAAB, with ordering alternated deterministically. The ratio uses the geometric mean of both legs for each implementation.
- A pair is retried when either implementation changes by more than eight percent between its two legs. At most five attempts are made, and forced final attempts are retained and counted rather than hidden.
- Eleven accepted trials are collected per point and comparison in each of nine independent sequential processes. No benchmark processes run concurrently.
- Point order is independently shuffled in every process. macOS interactive QoS succeeds. The advisory affinity-tag request returns status 46 (`KERN_NOT_SUPPORTED`), so this report makes no hard-pinning claim.
- Analysis first takes a median within each process, then a median across processes. Confidence intervals are deterministic 20,000-resample bootstraps over the nine process medians.
- A clear win requires the complete 95% interval below 0.99; a clear regression requires it above 1.01. Everything else is reported as indeterminate.

This design controls ordering drift, frequency transitions, interruptions, point-order bias, and false sample inflation. It does not eliminate shared-system noise, and it does not generalize M4 timings to another CPU.

## Assembly-level decimal analysis

Decimal is the primary path and was audited separately. The retained `jeaiii_main` implementation is stackless and contains no calls on its conversion paths. One- and two-digit values share a branchless first-pair lookup. Three through eight digits use bounded range kernels, and nine digits use a fixed kernel. Values with 10 through 16 digits use one reciprocal division by 10⁸, a bounded leading range, and one fixed eight-digit suffix. Values with 17 through 20 digits reuse two fixed eight-digit suffixes.

The complete native result is a clear win over `std::to_chars` at every decimal length. The fast_io/std ratios are 0.499–0.500x for one and two digits, 0.666–0.801x for three through eight digits, 0.726x for nine digits, 0.785x for ten digits, 0.941–0.959x for 11 through 16 digits, and 0.811–0.830x for 17 through 20 digits. The weakest point is 15 digits at 0.959x with a 95% interval of [0.953, 0.960].

The fixed-length kernel probes remove only the public length dispatcher; they retain reciprocal division, table-address generation, all pair loads, and all output stores. The standard-library 12- and 16-digit wrappers require a stack frame and an out-of-line 32-bit conversion call, while the fast_io kernels remain leaf functions.

| Decimal kernel | Static instructions | uOps/iteration | Apple M4/M1 throughput | Cortex-A76/A57 throughput |
|:---|---:|---:|---:|---:|
| 10 digits | 39 | 43 | 8.5 | 14.3 |
| 12 digits | 50 | 55 | 11.0 | 18.3 |
| 16 digits | 58 | 65 | 12.8 | 21.7 |

Two additional candidates were rejected. Exact-length prefix dispatch increased the helper from 310 to 329 instructions and introduced a 48-byte stack frame on the long path. A scalar one-digit shortcut improved several ranges but regressed two digits by about 20% and 19–20 digits by 2–3% in nine-process same-process A/B testing. The branchless first-pair and paired-range design was therefore retained.

## Assembly-level power-of-two optimization

The retained changes are confined to the AArch64 branches inside the existing power-of-two conversion function. Binary values through 16 digits compose existing four- and eight-digit tables without a scalar tail. Seven-digit octal values use one leading digit and two independent three-digit table copies. Two-digit hexadecimal values use the existing pair table; the previously retained bounded hexadecimal path covers the remaining short lengths.

The resolved-path MCA regions below include their range checks, address generation, loads, and stores. They are scheduling models rather than native timing claims.

| Path | Model | Old/new instructions | Old/new cycles | Old/new uOps | Old/new throughput |
|:---|:---|---:|---:|---:|:---|
| Binary, 14 digits | Apple M4/M1 | 36 / 25 | 1,012 / 539 | 42 / 26 | 9.5 / 5.3 |
| Binary, 14 digits | Cortex-A76/A57 | 36 / 25 | 1,707 / 908 | 46 / 26 | 16.0 / 8.7 |
| Octal, 7 digits | Apple M4/M1 | 39 / 24 | 1,037 / 614 | 44 / 24 | 10.3 / 6.0 |
| Octal, 7 digits | Cortex-A76/A57 | 39 / 24 | 1,804 / 809 | 48 / 24 | 16.0 / 8.0 |
| Hexadecimal, 2 digits | Apple M4/M1 | 20 / 11 | 458 / 261 | 20 / 12 | 4.5 / 2.5 |
| Hexadecimal, 2 digits | Cortex-A76/A57 | 20 / 11 | 706 / 409 | 20 / 12 | 7.0 / 4.0 |

### Apple M-series and traditional AArch64 separation

The seven-digit octal shortcut is now the only power-of-two kernel isolated by platform. It is enabled by `__APPLE__` together with the AArch64 target test. The one-through-six-digit octal kernels, all binary kernels, and all hexadecimal kernels remain shared AArch64 code because their resolved paths improve or preserve throughput on both Apple and Cortex/Neoverse models.

The separation was tested against the established implementation and against the common fallback independently. On Apple M4, seven alternating baseline/candidate process pairs used 4,096 deterministic values at every octal length and eleven calibrated trials per process. The retained shortcut measured `1.292 ns/value` at seven digits; forcing that range through the common fallback measured `2.559 ns/value`, or `1.981x` the shortcut time. The source snapshot tested in this section produces byte-identical resolved assembly for Apple M1, M2, M3, and M4; the M4 assembly SHA-256 is `b1e14697c78664db274f0cc72a805ba65b888adae1ed698fc01918c3942d3f33`.

For non-Apple AArch64, the shortcut is absent. The resolved seven-digit path is machine-word identical to the parent common implementation on Cortex-A57, Cortex-A76, Neoverse N1, and Neoverse V1. LLVM-MCA favors that common path on every traditional model tested:

| Model | Apple shortcut throughput | Common fallback throughput |
|:---|---:|---:|
| Cortex-A57 | 7.0 | 5.7 |
| Cortex-A76 | 7.0 | 5.7 |
| Neoverse N1 | 8.7 | 5.7 |
| Neoverse V1 | 3.3 | 2.3 |

Lower throughput is better. The Apple M1/M4 model also estimates `5.0` for the shortcut and `3.3` for the isolated fallback, but native M4 measures the opposite result. This is a useful model limitation: the resolved block omits the public dispatch context and cannot reproduce all front-end, table-locality, and layout effects. Native M4 measurement therefore selects the Apple path, while the traditional AArch64 decision uses the consistent A57/A76/Neoverse result. The earlier old/new octal row compares against the pre-optimization implementation, not against the newer common fallback used by this split.

An Apple-only 3+4 digit grouping was also tested. It reduced the resolved M4 kernel from 21 instructions and `5.0` throughput to 16 instructions and `3.5` throughput, but nine native process pairs showed no seven-digit improvement (`0.999x`) and a repeatable four-digit layout regression (`1.163x`). The candidate was rejected and the established 1+3+3 kernel was restored byte for byte.

Base 4 was investigated explicitly. A one-digit guard won that point in all 21 independent same-process runs, but repeatably cost 2% to 4% at established longer lengths. A 1-to-8-digit table path caused larger fallback regressions. Both candidates were rejected and the base-4 implementation was restored byte for byte.

Cross-compilation passes for Apple M1 through M4, Cortex-A57/A76, and Neoverse N1/V1. x86-64 objects are byte-identical to the saved baseline for x86-64, Core 2, Nehalem, Sandy Bridge, Haswell, Skylake, Skylake-AVX512, Cascade Lake, Ice Lake client, Tiger Lake, Alder Lake, Sapphire Rapids, and Zen 1 through 4.

## Reproduction artifacts

- `/tmp/fast_io_to_chars_bench.cpp`: `b722eee0f2ed01825470b6cd028292bf560a5f530358ab763083d3a3e709715c`.
- `/tmp/fast_io_to_chars_bench_m4_round2_final`: `cefa3c130b8a75942fead7a3d0d0593e60c58ca1cbfb9208f38475e412bbf13e`.
- `/tmp/fast_io_to_chars_paired_raw.csv`: `16b355904b4dc6a7773f23fcaa877d6722c3ca61c7b9a43bd0aa9f43c5d210a1`.
- `/tmp/fast_io_to_chars_paired_medians.csv`: `097b6355be02d45fd6779d25f4a3287c2431d26e6cff2fbb25740e3916fecb90`.
- `/tmp/fast_io_to_chars_paired_base_summary.csv`: `292a3bd5790c191a2c51a76324ddd28942ce9e35e4b4cd098be955f61cc36ca0`.
- `/tmp/fast_io_to_chars_correctness.cpp`: `9b0c3daf919d94e40e8e26a1a3ba5519411760dcf793ed554ec07f11f5e39c64`.
- `/tmp/fast_io_m4_hex_focused_bench.cpp`: `240603e609a4f579382d220dad79bef91bfc243a02732f460c0ed1c804232c04`.
- `/tmp/fast_io_power2_resolved_mca.csv`: `332a2f43f97b528266ba24113d4065b704946c44e209c338957aa11e8d8bcb3e`.
- `/tmp/fast_io_apple_aarch64_correctness.cpp`: `3bc5cb4d217114b24db7df15d511a82f6accde0873121491e0f0b3c89e1c7f1c`.

The benchmark was compiled with:

```sh
SYSROOT=$(xcrun --show-sdk-path)
clang++ -O3 --sysroot="$SYSROOT" -fuse-ld=lld -std=c++20 -Wno-c++23-extensions -march=native \
  -Iinclude -I/tmp/fast_io-to_chars-fmt-12.2.0/include \
  /tmp/fast_io_to_chars_bench.cpp /tmp/fast_io-to_chars-fmt-12.2.0/src/format.cc \
  -o /tmp/fast_io_to_chars_bench_m4_round2_final
```

## Statistical results

Across all 665 valid `(base, digits)` points, the geometric mean of the paired fast_io/std ratios is `0.575x`. fast_io has the lower point estimate at `662/665` points. Using a one-percent practical margin and a 95% process-level bootstrap interval, `648` points are clear wins, `0` are clear regressions, and `17` are indeterminate.

The best point is base 4, 31 digits at `0.091x`; the worst is base 26, 2 digits at `1.013x`. Across fmt's 122 natively supported points, fast_io/fmt has a geometric mean of `0.293x` and fast_io has the lower point estimate at `109/122` points.

The raw dataset contains `103,324` measured pairs. Drift filtering rejected and retried `25,411` pairs; `895` final attempts exceeded the drift limit after all retries.

## Results by base

The time columns are geometric means across valid digit lengths. Ratios below one favor fast_io. A clear result requires the complete 95% process-level interval to exceed the one-percent margin.

| Base | Points | Median wins | Clear wins | Clear regressions | fast_io ns | std ns | Ratio | Best | Worst |
|---:|---:|---:|---:|---:|---:|---:|---:|:---|:---|
| 2 | 64 | 64/64 | 61 | 0 | 3.063 | 4.922 | 0.620x | 0.396x at d61 | 0.957x at d20 |
| 3 | 41 | 41/41 | 39 | 0 | 6.988 | 16.470 | 0.422x | 0.286x at d41 | 0.923x at d2 |
| 4 | 32 | 31/32 | 31 | 0 | 2.853 | 11.310 | 0.251x | 0.091x at d31 | 1.002x at d1 |
| 5 | 28 | 28/28 | 27 | 0 | 5.753 | 10.683 | 0.539x | 0.389x at d27 | 0.909x at d2 |
| 6 | 25 | 25/25 | 24 | 0 | 4.725 | 9.501 | 0.494x | 0.286x at d25 | 0.919x at d2 |
| 7 | 23 | 23/23 | 22 | 0 | 5.011 | 8.750 | 0.572x | 0.420x at d23 | 0.909x at d2 |
| 8 | 22 | 22/22 | 22 | 0 | 2.714 | 4.585 | 0.596x | 0.392x at d12 | 0.897x at d3 |
| 9 | 21 | 21/21 | 21 | 0 | 4.612 | 8.029 | 0.575x | 0.434x at d21 | 0.910x at d2 |
| 10 | 20 | 20/20 | 20 | 0 | 2.360 | 3.019 | 0.782x | 0.499x at d2 | 0.959x at d15 |
| 11 | 19 | 19/19 | 19 | 0 | 4.712 | 7.336 | 0.641x | 0.496x at d19 | 0.884x at d2 |
| 12 | 18 | 18/18 | 18 | 0 | 3.958 | 7.001 | 0.565x | 0.360x at d17 | 0.848x at d2 |
| 13 | 18 | 18/18 | 18 | 0 | 4.230 | 6.979 | 0.599x | 0.465x at d17 | 0.847x at d2 |
| 14 | 17 | 17/17 | 17 | 0 | 3.948 | 6.741 | 0.587x | 0.381x at d17 | 0.921x at d2 |
| 15 | 17 | 17/17 | 17 | 0 | 4.080 | 6.690 | 0.610x | 0.465x at d17 | 0.911x at d2 |
| 16 | 16 | 15/16 | 10 | 0 | 2.643 | 3.340 | 0.787x | 0.389x at d9 | 1.005x at d5 |
| 17 | 16 | 16/16 | 16 | 0 | 3.842 | 6.364 | 0.601x | 0.416x at d16 | 0.846x at d2 |
| 18 | 16 | 16/16 | 16 | 0 | 3.857 | 6.402 | 0.605x | 0.412x at d16 | 0.935x at d2 |
| 19 | 16 | 16/16 | 16 | 0 | 3.910 | 6.330 | 0.613x | 0.417x at d16 | 0.912x at d2 |
| 20 | 15 | 15/15 | 15 | 0 | 3.720 | 6.121 | 0.605x | 0.416x at d15 | 0.850x at d2 |
| 21 | 15 | 15/15 | 15 | 0 | 3.668 | 6.060 | 0.608x | 0.415x at d15 | 0.846x at d2 |
| 22 | 15 | 15/15 | 15 | 0 | 3.769 | 6.068 | 0.622x | 0.419x at d15 | 0.897x at d1 |
| 23 | 15 | 15/15 | 15 | 0 | 3.680 | 6.035 | 0.608x | 0.413x at d15 | 0.848x at d2 |
| 24 | 14 | 14/14 | 14 | 0 | 3.664 | 5.744 | 0.634x | 0.446x at d13 | 0.916x at d2 |
| 25 | 14 | 14/14 | 14 | 0 | 3.680 | 5.763 | 0.637x | 0.436x at d13 | 0.896x at d2 |
| 26 | 14 | 13/14 | 13 | 0 | 3.778 | 5.740 | 0.655x | 0.453x at d13 | 1.013x at d2 |
| 27 | 14 | 14/14 | 14 | 0 | 3.675 | 5.744 | 0.639x | 0.441x at d13 | 0.911x at d2 |
| 28 | 14 | 14/14 | 14 | 0 | 3.622 | 5.757 | 0.623x | 0.440x at d13 | 0.845x at d2 |
| 29 | 14 | 14/14 | 14 | 0 | 3.971 | 5.762 | 0.690x | 0.529x at d14 | 0.900x at d2 |
| 30 | 14 | 14/14 | 14 | 0 | 3.695 | 5.795 | 0.641x | 0.463x at d13 | 0.891x at d2 |
| 31 | 13 | 13/13 | 13 | 0 | 3.528 | 5.480 | 0.638x | 0.438x at d13 | 0.919x at d2 |
| 32 | 13 | 13/13 | 12 | 0 | 2.924 | 5.268 | 0.558x | 0.324x at d13 | 0.946x at d2 |
| 33 | 13 | 13/13 | 13 | 0 | 3.523 | 5.467 | 0.642x | 0.440x at d13 | 0.891x at d2 |
| 34 | 13 | 13/13 | 13 | 0 | 3.717 | 5.496 | 0.673x | 0.505x at d12 | 0.864x at d4 |
| 35 | 13 | 13/13 | 13 | 0 | 3.481 | 5.543 | 0.634x | 0.439x at d13 | 0.875x at d2 |
| 36 | 13 | 13/13 | 13 | 0 | 3.539 | 5.452 | 0.645x | 0.441x at d13 | 0.920x at d2 |

## Decimal results by length

| Digits | fast_io ns | std ns | fast/std | 95% interval | Classification | fast/fmt |
|---:|---:|---:|---:|:---|:---|---:|
| 1 | 1.016 | 2.029 | 0.500x | [0.499, 0.501] | clear win | 1.260x |
| 2 | 1.011 | 2.028 | 0.499x | [0.499, 0.500] | clear win | 1.261x |
| 3 | 1.528 | 2.283 | 0.666x | [0.666, 0.672] | clear win | 1.418x |
| 4 | 1.526 | 2.288 | 0.667x | [0.665, 0.668] | clear win | 1.443x |
| 5 | 1.778 | 2.279 | 0.778x | [0.777, 0.779] | clear win | 1.096x |
| 6 | 1.773 | 2.531 | 0.700x | [0.699, 0.701] | clear win | 1.121x |
| 7 | 2.026 | 2.528 | 0.800x | [0.798, 0.802] | clear win | 1.008x |
| 8 | 2.034 | 2.542 | 0.801x | [0.799, 0.803] | clear win | 1.100x |
| 9 | 2.025 | 2.793 | 0.726x | [0.726, 0.729] | clear win | 0.853x |
| 10 | 2.542 | 3.255 | 0.785x | [0.761, 0.805] | clear win | 1.144x |
| 11 | 3.051 | 3.227 | 0.947x | [0.945, 0.949] | clear win | 1.136x |
| 12 | 3.060 | 3.252 | 0.941x | [0.938, 0.943] | clear win | 1.166x |
| 13 | 3.306 | 3.500 | 0.944x | [0.939, 0.946] | clear win | 1.058x |
| 14 | 3.298 | 3.482 | 0.947x | [0.943, 0.951] | clear win | 1.080x |
| 15 | 3.544 | 3.712 | 0.959x | [0.953, 0.960] | clear win | 0.985x |
| 16 | 3.546 | 3.735 | 0.949x | [0.946, 0.951] | clear win | 0.999x |
| 17 | 3.314 | 4.084 | 0.811x | [0.810, 0.812] | clear win | 0.788x |
| 18 | 3.306 | 4.046 | 0.821x | [0.817, 0.824] | clear win | 0.800x |
| 19 | 3.589 | 4.330 | 0.828x | [0.820, 0.829] | clear win | 0.744x |
| 20 | 3.580 | 4.308 | 0.830x | [0.823, 0.833] | clear win | 0.782x |

## Complete fast_io/std matrix

Each entry is `digit-length:ratio[95% interval]`; no valid `uint64_t` length is omitted.

| Base | Paired fast_io/std ratios |
|---:|:---|
| 2 | d1:0.777[0.701,0.799] d2:0.820[0.791,0.898] d3:0.795[0.750,0.854] d4:0.823[0.750,0.835] d5:0.729[0.712,0.800] d6:0.819[0.730,0.865] d7:0.913[0.831,1.000] d8:0.833[0.811,0.865] d9:0.799[0.739,0.888] d10:0.906[0.822,0.908] d11:0.918[0.884,0.999] d12:0.913[0.848,0.933] d13:0.763[0.672,0.832] d14:0.915[0.900,0.999] d15:0.919[0.858,0.932] d16:0.916[0.912,0.972] d17:0.560[0.514,0.575] d18:0.916[0.914,0.976] d19:0.945[0.914,0.986] d20:0.957[0.929,0.982] d21:0.566[0.531,0.618] d22:0.839[0.835,0.848] d23:0.811[0.796,0.854] d24:0.751[0.746,0.808] d25:0.543[0.520,0.574] d26:0.861[0.800,0.863] d27:0.839[0.774,0.841] d28:0.558[0.469,0.705] d29:0.503[0.497,0.545] d30:0.718[0.701,0.766] d31:0.558[0.519,0.585] d32:0.485[0.456,0.578] d33:0.491[0.452,0.495] d34:0.563[0.525,0.582] d35:0.553[0.419,0.603] d36:0.442[0.403,0.461] d37:0.440[0.435,0.483] d38:0.496[0.439,0.547] d39:0.499[0.479,0.579] d40:0.567[0.481,0.589] d41:0.496[0.458,0.521] d42:0.501[0.431,0.579] d43:0.544[0.405,0.570] d44:0.577[0.561,0.604] d45:0.468[0.441,0.491] d46:0.522[0.494,0.565] d47:0.542[0.524,0.580] d48:0.559[0.539,0.595] d49:0.478[0.461,0.519] d50:0.549[0.502,0.574] d51:0.546[0.530,0.590] d52:0.609[0.555,0.617] d53:0.478[0.456,0.497] d54:0.500[0.470,0.517] d55:0.507[0.482,0.534] d56:0.517[0.500,0.552] d57:0.415[0.406,0.443] d58:0.458[0.445,0.482] d59:0.505[0.480,0.518] d60:0.517[0.501,0.534] d61:0.396[0.386,0.428] d62:0.456[0.440,0.465] d63:0.481[0.462,0.495] d64:0.491[0.468,0.499] |
| 3 | d1:0.900[0.892,0.997] d2:0.923[0.922,0.991] d3:0.784[0.754,0.787] d4:0.773[0.732,0.800] d5:0.781[0.743,0.803] d6:0.817[0.764,0.866] d7:0.658[0.623,0.669] d8:0.566[0.555,0.602] d9:0.580[0.555,0.591] d10:0.621[0.578,0.658] d11:0.577[0.529,0.586] d12:0.552[0.536,0.567] d13:0.493[0.469,0.495] d14:0.504[0.499,0.524] d15:0.460[0.438,0.462] d16:0.423[0.422,0.452] d17:0.401[0.399,0.403] d18:0.413[0.409,0.433] d19:0.384[0.382,0.387] d20:0.376[0.375,0.384] d21:0.361[0.360,0.363] d22:0.363[0.360,0.374] d23:0.340[0.339,0.342] d24:0.338[0.336,0.340] d25:0.328[0.327,0.328] d26:0.328[0.326,0.329] d27:0.316[0.315,0.317] d28:0.315[0.313,0.316] d29:0.318[0.318,0.319] d30:0.319[0.319,0.322] d31:0.306[0.306,0.307] d32:0.308[0.307,0.309] d33:0.309[0.308,0.309] d34:0.309[0.308,0.311] d35:0.297[0.296,0.297] d36:0.297[0.296,0.298] d37:0.294[0.293,0.295] d38:0.297[0.295,0.297] d39:0.290[0.289,0.292] d40:0.288[0.286,0.288] d41:0.286[0.285,0.288] |
| 4 | d1:1.002[0.995,1.054] d2:0.857[0.843,0.925] d3:0.714[0.692,0.725] d4:0.737[0.668,0.800] d5:0.674[0.605,0.736] d6:0.654[0.603,0.702] d7:0.529[0.470,0.563] d8:0.451[0.449,0.488] d9:0.363[0.334,0.369] d10:0.382[0.349,0.384] d11:0.298[0.295,0.331] d12:0.326[0.309,0.340] d13:0.296[0.271,0.297] d14:0.297[0.274,0.299] d15:0.232[0.230,0.252] d16:0.255[0.236,0.256] d17:0.211[0.192,0.212] d18:0.215[0.199,0.218] d19:0.187[0.170,0.187] d20:0.192[0.177,0.193] d21:0.173[0.159,0.174] d22:0.165[0.163,0.177] d23:0.152[0.141,0.153] d24:0.156[0.145,0.156] d25:0.121[0.112,0.122] d26:0.126[0.116,0.127] d27:0.100[0.099,0.111] d28:0.115[0.107,0.115] d29:0.099[0.099,0.107] d30:0.112[0.104,0.113] d31:0.091[0.090,0.099] d32:0.097[0.095,0.104] |
| 5 | d1:0.903[0.835,0.996] d2:0.909[0.863,0.923] d3:0.782[0.721,0.797] d4:0.736[0.732,0.801] d5:0.739[0.670,0.760] d6:0.816[0.762,0.819] d7:0.643[0.580,0.652] d8:0.582[0.551,0.593] d9:0.550[0.543,0.571] d10:0.625[0.621,0.647] d11:0.556[0.549,0.581] d12:0.560[0.557,0.562] d13:0.504[0.500,0.505] d14:0.542[0.539,0.545] d15:0.482[0.481,0.485] d16:0.495[0.494,0.496] d17:0.452[0.451,0.456] d18:0.481[0.481,0.485] d19:0.446[0.443,0.447] d20:0.472[0.472,0.473] d21:0.428[0.426,0.429] d22:0.453[0.451,0.453] d23:0.412[0.410,0.414] d24:0.424[0.422,0.424] d25:0.400[0.399,0.402] d26:0.411[0.411,0.414] d27:0.389[0.388,0.394] d28:0.407[0.405,0.410] |
| 6 | d1:0.898[0.816,0.999] d2:0.919[0.908,0.980] d3:0.768[0.712,0.785] d4:0.736[0.733,0.800] d5:0.741[0.650,0.801] d6:0.749[0.719,0.870] d7:0.640[0.587,0.686] d8:0.565[0.532,0.604] d9:0.544[0.514,0.564] d10:0.621[0.586,0.652] d11:0.553[0.517,0.579] d12:0.529[0.504,0.558] d13:0.459[0.457,0.483] d14:0.488[0.467,0.508] d15:0.441[0.412,0.449] d16:0.417[0.403,0.431] d17:0.363[0.362,0.379] d18:0.415[0.396,0.431] d19:0.376[0.367,0.387] d20:0.362[0.348,0.374] d21:0.309[0.302,0.335] d22:0.348[0.327,0.350] d23:0.300[0.295,0.311] d24:0.306[0.300,0.314] d25:0.286[0.284,0.295] |
| 7 | d1:0.902[0.893,0.996] d2:0.909[0.862,0.921] d3:0.786[0.711,0.789] d4:0.798[0.744,0.803] d5:0.738[0.648,0.743] d6:0.807[0.742,0.809] d7:0.578[0.569,0.687] d8:0.577[0.561,0.639] d9:0.552[0.542,0.581] d10:0.621[0.616,0.629] d11:0.566[0.548,0.579] d12:0.552[0.550,0.553] d13:0.495[0.494,0.512] d14:0.530[0.530,0.533] d15:0.477[0.474,0.480] d16:0.491[0.490,0.492] d17:0.457[0.452,0.459] d18:0.482[0.481,0.483] d19:0.446[0.444,0.447] d20:0.475[0.473,0.476] d21:0.435[0.432,0.437] d22:0.454[0.453,0.456] d23:0.420[0.419,0.421] |
| 8 | d1:0.601[0.597,0.698] d2:0.701[0.632,0.725] d3:0.897[0.780,0.900] d4:0.707[0.641,0.736] d5:0.811[0.749,0.821] d6:0.834[0.759,0.915] d7:0.693[0.662,0.747] d8:0.831[0.642,0.857] d9:0.455[0.393,0.768] d10:0.531[0.491,0.627] d11:0.481[0.472,0.582] d12:0.392[0.355,0.420] d13:0.523[0.509,0.573] d14:0.498[0.449,0.501] d15:0.638[0.599,0.658] d16:0.483[0.465,0.509] d17:0.547[0.504,0.554] d18:0.551[0.540,0.594] d19:0.587[0.585,0.635] d20:0.566[0.534,0.580] d21:0.520[0.479,0.563] d22:0.563[0.557,0.565] |
| 9 | d1:0.800[0.794,0.881] d2:0.910[0.840,0.925] d3:0.717[0.644,0.776] d4:0.734[0.675,0.796] d5:0.742[0.635,0.800] d6:0.753[0.695,0.804] d7:0.600[0.567,0.644] d8:0.570[0.525,0.597] d9:0.565[0.545,0.581] d10:0.621[0.617,0.630] d11:0.542[0.539,0.555] d12:0.543[0.542,0.547] d13:0.492[0.491,0.494] d14:0.528[0.528,0.532] d15:0.473[0.472,0.475] d16:0.491[0.488,0.492] d17:0.455[0.454,0.457] d18:0.482[0.481,0.486] d19:0.447[0.446,0.448] d20:0.470[0.467,0.471] d21:0.434[0.433,0.436] |
| 10 | d1:0.500[0.499,0.501] d2:0.499[0.499,0.500] d3:0.666[0.666,0.672] d4:0.667[0.665,0.668] d5:0.778[0.777,0.779] d6:0.700[0.699,0.701] d7:0.800[0.798,0.802] d8:0.801[0.799,0.803] d9:0.726[0.726,0.729] d10:0.785[0.761,0.805] d11:0.947[0.945,0.949] d12:0.941[0.938,0.943] d13:0.944[0.939,0.946] d14:0.947[0.943,0.951] d15:0.959[0.953,0.960] d16:0.949[0.946,0.951] d17:0.811[0.810,0.812] d18:0.821[0.817,0.824] d19:0.828[0.820,0.829] d20:0.830[0.823,0.833] |
| 11 | d1:0.803[0.799,0.841] d2:0.884[0.846,0.931] d3:0.777[0.711,0.841] d4:0.802[0.798,0.865] d5:0.796[0.688,0.855] d6:0.768[0.711,0.830] d7:0.746[0.691,0.822] d8:0.567[0.520,0.606] d9:0.708[0.653,0.728] d10:0.609[0.606,0.648] d11:0.660[0.626,0.690] d12:0.574[0.572,0.578] d13:0.554[0.552,0.566] d14:0.559[0.554,0.564] d15:0.522[0.516,0.525] d16:0.522[0.521,0.525] d17:0.514[0.513,0.514] d18:0.532[0.530,0.533] d19:0.496[0.495,0.498] |
| 12 | d1:0.804[0.794,0.898] d2:0.848[0.768,0.908] d3:0.753[0.710,0.786] d4:0.731[0.668,0.777] d5:0.708[0.632,0.784] d6:0.752[0.709,0.833] d7:0.638[0.573,0.684] d8:0.562[0.529,0.589] d9:0.541[0.528,0.578] d10:0.618[0.579,0.646] d11:0.537[0.503,0.562] d12:0.508[0.508,0.527] d13:0.447[0.442,0.470] d14:0.497[0.475,0.497] d15:0.416[0.398,0.434] d16:0.403[0.395,0.418] d17:0.360[0.349,0.377] d18:0.394[0.381,0.418] |
| 13 | d1:0.803[0.702,0.901] d2:0.847[0.820,0.918] d3:0.770[0.714,0.780] d4:0.734[0.723,0.793] d5:0.678[0.641,0.786] d6:0.797[0.758,0.846] d7:0.600[0.588,0.644] d8:0.576[0.522,0.580] d9:0.553[0.537,0.577] d10:0.622[0.613,0.644] d11:0.538[0.535,0.562] d12:0.541[0.538,0.547] d13:0.504[0.501,0.506] d14:0.525[0.524,0.526] d15:0.484[0.481,0.487] d16:0.489[0.486,0.491] d17:0.465[0.464,0.466] d18:0.477[0.474,0.480] |
| 14 | d1:0.893[0.800,0.901] d2:0.921[0.841,0.944] d3:0.785[0.755,0.795] d4:0.737[0.667,0.798] d5:0.687[0.631,0.772] d6:0.757[0.716,0.848] d7:0.602[0.567,0.677] d8:0.563[0.551,0.595] d9:0.569[0.538,0.578] d10:0.611[0.606,0.641] d11:0.531[0.509,0.559] d12:0.510[0.504,0.538] d13:0.445[0.443,0.463] d14:0.500[0.479,0.533] d15:0.431[0.414,0.448] d16:0.404[0.403,0.422] d17:0.381[0.377,0.387] |
| 15 | d1:0.801[0.704,0.893] d2:0.911[0.840,0.927] d3:0.764[0.714,0.776] d4:0.733[0.709,0.800] d5:0.732[0.673,0.738] d6:0.798[0.738,0.812] d7:0.580[0.527,0.637] d8:0.561[0.530,0.600] d9:0.545[0.537,0.574] d10:0.615[0.603,0.644] d11:0.550[0.533,0.559] d12:0.535[0.534,0.537] d13:0.503[0.502,0.504] d14:0.525[0.524,0.529] d15:0.486[0.483,0.488] d16:0.488[0.483,0.489] d17:0.465[0.464,0.466] |
| 16 | d1:0.873[0.784,0.893] d2:0.899[0.872,0.966] d3:0.994[0.890,1.108] d4:0.911[0.840,0.960] d5:1.005[0.898,1.101] d6:0.970[0.894,1.012] d7:0.830[0.415,0.908] d8:0.910[0.901,0.992] d9:0.389[0.326,0.393] d10:0.750[0.713,0.789] d11:0.502[0.482,0.563] d12:0.926[0.920,0.994] d13:0.633[0.606,0.648] d14:0.927[0.854,0.994] d15:0.610[0.597,0.660] d16:0.861[0.833,0.935] |
| 17 | d1:0.810[0.796,0.898] d2:0.846[0.840,0.920] d3:0.735[0.710,0.767] d4:0.801[0.774,0.869] d5:0.714[0.674,0.776] d6:0.797[0.755,0.843] d7:0.596[0.525,0.638] d8:0.578[0.555,0.607] d9:0.569[0.542,0.576] d10:0.623[0.609,0.645] d11:0.532[0.508,0.560] d12:0.516[0.491,0.533] d13:0.441[0.420,0.465] d14:0.473[0.466,0.493] d15:0.434[0.405,0.435] d16:0.416[0.400,0.426] |
| 18 | d1:0.802[0.790,0.898] d2:0.935[0.915,0.988] d3:0.782[0.715,0.804] d4:0.803[0.742,0.849] d5:0.732[0.668,0.784] d6:0.755[0.704,0.812] d7:0.596[0.563,0.640] d8:0.563[0.533,0.590] d9:0.548[0.536,0.574] d10:0.610[0.601,0.641] d11:0.538[0.528,0.559] d12:0.532[0.487,0.536] d13:0.457[0.440,0.465] d14:0.474[0.469,0.496] d15:0.434[0.412,0.434] d16:0.412[0.403,0.417] |
| 19 | d1:0.891[0.798,0.900] d2:0.912[0.837,0.933] d3:0.763[0.698,0.785] d4:0.802[0.747,0.863] d5:0.732[0.680,0.734] d6:0.803[0.719,0.843] d7:0.603[0.560,0.679] d8:0.590[0.570,0.592] d9:0.545[0.538,0.565] d10:0.623[0.605,0.641] d11:0.532[0.529,0.561] d12:0.527[0.503,0.531] d13:0.453[0.442,0.467] d14:0.495[0.474,0.496] d15:0.431[0.413,0.436] d16:0.417[0.398,0.432] |
| 20 | d1:0.802[0.781,0.895] d2:0.850[0.846,0.904] d3:0.716[0.711,0.782] d4:0.743[0.731,0.793] d5:0.676[0.632,0.778] d6:0.761[0.750,0.848] d7:0.602[0.535,0.663] d8:0.572[0.554,0.592] d9:0.565[0.533,0.571] d10:0.611[0.596,0.633] d11:0.542[0.507,0.558] d12:0.506[0.479,0.530] d13:0.443[0.440,0.466] d14:0.483[0.476,0.497] d15:0.416[0.415,0.426] |
| 21 | d1:0.801[0.795,0.887] d2:0.846[0.771,0.864] d3:0.710[0.646,0.767] d4:0.799[0.776,0.862] d5:0.729[0.633,0.783] d6:0.752[0.709,0.790] d7:0.628[0.590,0.638] d8:0.563[0.516,0.577] d9:0.544[0.535,0.560] d10:0.609[0.580,0.636] d11:0.528[0.501,0.559] d12:0.504[0.477,0.518] d13:0.441[0.419,0.444] d14:0.473[0.464,0.495] d15:0.415[0.395,0.434] |
| 22 | d1:0.897[0.800,0.900] d2:0.863[0.773,0.921] d3:0.784[0.713,0.785] d4:0.860[0.803,0.867] d5:0.730[0.681,0.791] d6:0.712[0.707,0.840] d7:0.637[0.561,0.679] d8:0.561[0.548,0.605] d9:0.535[0.527,0.564] d10:0.641[0.609,0.644] d11:0.530[0.526,0.561] d12:0.504[0.501,0.520] d13:0.444[0.441,0.464] d14:0.494[0.472,0.511] d15:0.419[0.415,0.434] |
| 23 | d1:0.801[0.716,0.900] d2:0.848[0.844,0.890] d3:0.719[0.693,0.784] d4:0.806[0.733,0.866] d5:0.673[0.627,0.719] d6:0.784[0.745,0.805] d7:0.635[0.564,0.679] d8:0.565[0.555,0.589] d9:0.535[0.500,0.549] d10:0.612[0.577,0.622] d11:0.528[0.501,0.559] d12:0.507[0.493,0.530] d13:0.442[0.417,0.464] d14:0.480[0.455,0.490] d15:0.413[0.402,0.420] |
| 24 | d1:0.806[0.800,0.900] d2:0.916[0.878,0.925] d3:0.787[0.715,0.847] d4:0.735[0.701,0.744] d5:0.731[0.686,0.794] d6:0.757[0.741,0.842] d7:0.635[0.595,0.657] d8:0.586[0.553,0.629] d9:0.536[0.532,0.569] d10:0.610[0.579,0.645] d11:0.547[0.528,0.560] d12:0.506[0.483,0.534] d13:0.446[0.438,0.465] d14:0.474[0.466,0.495] |
| 25 | d1:0.803[0.705,0.898] d2:0.896[0.845,0.920] d3:0.716[0.649,0.775] d4:0.816[0.734,0.867] d5:0.735[0.667,0.737] d6:0.791[0.740,0.846] d7:0.619[0.559,0.672] d8:0.584[0.549,0.587] d9:0.543[0.501,0.570] d10:0.616[0.582,0.643] d11:0.556[0.527,0.559] d12:0.518[0.506,0.530] d13:0.436[0.415,0.458] d14:0.493[0.462,0.499] |
| 26 | d1:0.803[0.703,0.896] d2:1.013[0.994,1.040] d3:0.852[0.847,0.870] d4:0.801[0.733,0.865] d5:0.736[0.672,0.797] d6:0.787[0.699,0.848] d7:0.631[0.563,0.664] d8:0.584[0.547,0.592] d9:0.566[0.504,0.571] d10:0.648[0.619,0.668] d11:0.532[0.510,0.541] d12:0.508[0.493,0.531] d13:0.453[0.439,0.464] d14:0.506[0.476,0.534] |
| 27 | d1:0.892[0.800,0.902] d2:0.911[0.850,0.923] d3:0.743[0.716,0.759] d4:0.802[0.797,0.867] d5:0.686[0.672,0.741] d6:0.788[0.745,0.813] d7:0.627[0.535,0.673] d8:0.598[0.547,0.647] d9:0.533[0.529,0.540] d10:0.612[0.605,0.640] d11:0.532[0.528,0.557] d12:0.530[0.502,0.531] d13:0.441[0.440,0.461] d14:0.478[0.470,0.495] |
| 28 | d1:0.795[0.704,0.875] d2:0.845[0.827,0.917] d3:0.713[0.695,0.778] d4:0.797[0.749,0.804] d5:0.682[0.624,0.733] d6:0.758[0.745,0.798] d7:0.594[0.556,0.666] d8:0.559[0.546,0.571] d9:0.540[0.533,0.571] d10:0.638[0.608,0.644] d11:0.531[0.504,0.559] d12:0.511[0.502,0.531] d13:0.440[0.428,0.465] d14:0.496[0.474,0.512] |
| 29 | d1:0.894[0.799,0.897] d2:0.900[0.858,0.980] d3:0.782[0.710,0.834] d4:0.803[0.799,0.863] d5:0.753[0.685,0.794] d6:0.786[0.718,0.833] d7:0.758[0.684,0.803] d8:0.581[0.555,0.643] d9:0.692[0.675,0.711] d10:0.610[0.586,0.628] d11:0.654[0.631,0.681] d12:0.537[0.535,0.538] d13:0.537[0.533,0.560] d14:0.529[0.528,0.531] |
| 30 | d1:0.802[0.797,0.900] d2:0.891[0.847,0.924] d3:0.768[0.713,0.786] d4:0.809[0.779,0.868] d5:0.732[0.625,0.792] d6:0.790[0.703,0.842] d7:0.593[0.561,0.652] d8:0.577[0.550,0.647] d9:0.543[0.534,0.570] d10:0.637[0.608,0.644] d11:0.551[0.531,0.560] d12:0.519[0.503,0.530] d13:0.463[0.439,0.464] d14:0.489[0.476,0.503] |
| 31 | d1:0.798[0.703,0.897] d2:0.919[0.844,0.922] d3:0.716[0.648,0.760] d4:0.832[0.804,0.871] d5:0.679[0.640,0.732] d6:0.763[0.705,0.798] d7:0.596[0.571,0.649] d8:0.549[0.544,0.603] d9:0.535[0.530,0.568] d10:0.611[0.602,0.640] d11:0.541[0.500,0.558] d12:0.505[0.480,0.529] d13:0.438[0.414,0.463] |
| 32 | d1:0.808[0.782,0.900] d2:0.946[0.868,1.004] d3:0.857[0.744,0.914] d4:0.798[0.767,0.801] d5:0.679[0.628,0.689] d6:0.654[0.547,0.660] d7:0.505[0.436,0.524] d8:0.463[0.424,0.512] d9:0.456[0.412,0.482] d10:0.439[0.401,0.476] d11:0.396[0.367,0.425] d12:0.368[0.343,0.372] d13:0.324[0.299,0.351] |
| 33 | d1:0.799[0.702,0.801] d2:0.891[0.846,0.926] d3:0.784[0.778,0.787] d4:0.803[0.780,0.868] d5:0.676[0.664,0.738] d6:0.794[0.733,0.843] d7:0.591[0.548,0.669] d8:0.581[0.530,0.604] d9:0.538[0.507,0.570] d10:0.610[0.607,0.641] d11:0.527[0.505,0.528] d12:0.503[0.501,0.507] d13:0.440[0.437,0.461] |
| 34 | d1:0.700[0.698,0.792] d2:0.847[0.846,0.922] d3:0.681[0.649,0.713] d4:0.864[0.799,0.867] d5:0.712[0.631,0.791] d6:0.766[0.705,0.846] d7:0.708[0.669,0.830] d8:0.583[0.546,0.643] d9:0.679[0.653,0.694] d10:0.613[0.609,0.643] d11:0.655[0.650,0.679] d12:0.505[0.502,0.528] d13:0.537[0.536,0.558] |
| 35 | d1:0.799[0.701,0.893] d2:0.875[0.824,0.924] d3:0.727[0.713,0.777] d4:0.805[0.796,0.831] d5:0.700[0.633,0.788] d6:0.752[0.706,0.807] d7:0.588[0.517,0.667] d8:0.549[0.545,0.584] d9:0.539[0.532,0.550] d10:0.607[0.579,0.640] d11:0.527[0.509,0.529] d12:0.503[0.499,0.530] d13:0.439[0.430,0.441] |
| 36 | d1:0.800[0.710,0.899] d2:0.920[0.873,0.925] d3:0.766[0.712,0.786] d4:0.803[0.799,0.866] d5:0.728[0.666,0.779] d6:0.788[0.717,0.793] d7:0.615[0.548,0.662] d8:0.549[0.544,0.600] d9:0.536[0.509,0.547] d10:0.608[0.598,0.636] d11:0.527[0.524,0.539] d12:0.504[0.501,0.542] d13:0.441[0.439,0.455] |

## Complete fast_io/fmt matrix

fmt 12.2.0 provides the comparable core integer path for bases 2, 8, 10, and 16.

| Base | Paired fast_io/fmt ratios |
|---:|:---|
| 2 | d1:0.636[0.609,0.666] d2:0.714[0.663,0.749] d3:0.603[0.569,0.651] d4:0.572[0.554,0.608] d5:0.412[0.379,0.446] d6:0.380[0.378,0.398] d7:0.443[0.413,0.480] d8:0.371[0.368,0.399] d9:0.327[0.314,0.363] d10:0.294[0.268,0.304] d11:0.323[0.299,0.341] d12:0.319[0.304,0.336] d13:0.250[0.220,0.274] d14:0.261[0.249,0.280] d15:0.254[0.246,0.279] d16:0.292[0.274,0.304] d17:0.240[0.226,0.255] d18:0.240[0.232,0.252] d19:0.255[0.250,0.264] d20:0.264[0.245,0.273] d21:0.203[0.195,0.212] d22:0.191[0.181,0.200] d23:0.200[0.197,0.210] d24:0.206[0.202,0.219] d25:0.177[0.166,0.185] d26:0.191[0.178,0.193] d27:0.199[0.185,0.202] d28:0.198[0.192,0.212] d29:0.156[0.148,0.162] d30:0.170[0.158,0.172] d31:0.179[0.166,0.180] d32:0.179[0.175,0.187] d33:0.124[0.121,0.135] d34:0.134[0.132,0.144] d35:0.141[0.135,0.152] d36:0.149[0.148,0.160] d37:0.112[0.111,0.123] d38:0.126[0.120,0.131] d39:0.138[0.130,0.139] d40:0.139[0.136,0.146] d41:0.119[0.108,0.122] d42:0.121[0.119,0.131] d43:0.128[0.117,0.137] d44:0.136[0.134,0.142] d45:0.106[0.103,0.114] d46:0.114[0.111,0.120] d47:0.126[0.118,0.128] d48:0.126[0.118,0.134] d49:0.106[0.104,0.108] d50:0.112[0.104,0.120] d51:0.115[0.111,0.120] d52:0.128[0.123,0.132] d53:0.100[0.097,0.107] d54:0.104[0.095,0.105] d55:0.105[0.104,0.111] d56:0.110[0.108,0.115] d57:0.085[0.083,0.090] d58:0.092[0.091,0.099] d59:0.103[0.097,0.105] d60:0.104[0.103,0.111] d61:0.081[0.079,0.085] d62:0.089[0.087,0.093] d63:0.095[0.093,0.097] d64:0.186[0.181,0.191] |
| 8 | d1:0.544[0.526,0.636] d2:0.568[0.539,0.586] d3:0.533[0.499,0.543] d4:0.420[0.394,0.439] d5:0.433[0.425,0.490] d6:0.400[0.388,0.431] d7:0.320[0.259,0.363] d8:0.395[0.369,0.421] d9:0.377[0.373,0.387] d10:0.385[0.343,0.418] d11:0.385[0.343,0.399] d12:0.292[0.263,0.320] d13:0.281[0.268,0.297] d14:0.293[0.279,0.311] d15:0.298[0.283,0.310] d16:0.245[0.229,0.261] d17:0.244[0.219,0.248] d18:0.235[0.215,0.255] d19:0.248[0.241,0.259] d20:0.231[0.218,0.236] d21:0.208[0.192,0.214] d22:0.217[0.210,0.220] |
| 10 | d1:1.260[1.249,1.263] d2:1.261[1.252,1.263] d3:1.418[1.411,1.424] d4:1.443[1.429,1.447] d5:1.096[1.090,1.099] d6:1.121[1.118,1.126] d7:1.008[0.965,1.032] d8:1.100[1.097,1.104] d9:0.853[0.852,0.856] d10:1.144[1.124,1.151] d11:1.136[1.133,1.142] d12:1.166[1.146,1.170] d13:1.058[1.054,1.060] d14:1.080[1.074,1.086] d15:0.985[0.983,0.986] d16:0.999[0.997,1.003] d17:0.788[0.783,0.792] d18:0.800[0.794,0.802] d19:0.744[0.743,0.748] d20:0.782[0.778,0.789] |
| 16 | d1:0.584[0.580,0.666] d2:0.643[0.617,0.692] d3:0.529[0.469,0.588] d4:0.443[0.427,0.476] d5:0.433[0.388,0.477] d6:0.400[0.373,0.408] d7:0.370[0.367,0.381] d8:0.347[0.334,0.377] d9:0.322[0.286,0.353] d10:0.273[0.261,0.296] d11:0.311[0.303,0.327] d12:0.341[0.318,0.351] d13:0.310[0.293,0.319] d14:0.317[0.302,0.332] d15:0.271[0.266,0.292] d16:0.279[0.276,0.290] |


---

# x86_64 `fast_io::to_chars` rerun benchmark on Intel Core i9-14900HK class

This rerun was collected after the `git pull` conflict-resolution pass and the retained x86_64 runtime-base short-path patch. The requested platform label remains `i9-14900HK`; the local Linux host reports `Intel(R) Core(TM) i9-14900HX`. The benchmark was run at `2026-07-12 21:33:05 local time` and pinned with `taskset -c 4`.

The upstream `jeaiii/itoa` checkout used for the decimal comparison is commit `69308f65e87a9954f11f952ed04d551eabeee0ae`. The fast_io checkout HEAD during the run was `40a01573885998d4a34e310f6e52377483569eb4` with the local `int_to_chars.h` patch applied. Every timed point performed a byte-for-byte preflight against `std::to_chars`; decimal points were also checked against fast_io direct `jeaiii_main` and upstream `jeaiii::to_text_from_integer`.

Measurement shape: `2048` deterministic `uint64_t` values per `(base, digits)` point, `32` repetitions per trial, and `5` trials with the median reported. The complete matrix contains `665` valid points across bases 2 through 36, from one digit through the maximum `uint64_t` digit length for each base.

Across the complete matrix, the geometric mean is `0.440x` for fast_io/std (`4.729 ns` vs `10.743 ns`). fast_io is faster at `649/665` points, std is faster at `16/665` points, and exact ties occur at `0` points. The best fast_io point is base 4 digit 31 at `0.048x`; the weakest point is base 11 digit 6 at `1.180x`.

The benchmark was compiled with:

```bash
clang++ -std=c++23 -O3 -DNDEBUG -march=native -fno-exceptions -fno-rtti \
  -Iinclude -I/tmp/jeaiii_itoa/include \
  /tmp/fast_io_to_chars_rerun_bench.cpp -o /tmp/fast_io_to_chars_rerun_bench
taskset -c 4 /tmp/fast_io_to_chars_rerun_bench > /tmp/fast_io_to_chars_rerun_bench.csv
```

Compiler: `clang version 23.0.0git (https://github.com/llvm/llvm-project.git 4c4c1db7c69a6fda6cfa6bc6066bb09a433edc89)`.

Artifacts:

- `/tmp/fast_io_to_chars_rerun_bench.cpp`: `20e60d4155f332f291c54569c9498a3070e13b94c9753cc2683ff3240cf553e6`.
- `/tmp/fast_io_to_chars_rerun_bench`: `7ed598962dce9623f8ecd3aac47bfedecefb2dcf95400acf9da8cadfc69e156b`.
- `/tmp/fast_io_to_chars_rerun_bench.csv`: `e7dcf436be41da6ad129fc8965f066bb6df79137dfdbb8fe3efeecccf1911f91`.
- `/tmp/fast_io_to_chars_rerun_bench.stderr`: `b7cccb95a8ea51e493b47540af067039f40779e120c0e7e87630d39483e5b613`.
- `/tmp/jeaiii_itoa/include/itoa/jeaiii_to_text.h`: `9ed6427a753e944369b4103c70e4f73bf11a9a092faf5c9ef8b45ae751ea0311`.
- Benchmark stderr: `checksum=64965730880`.

## Rerun Results By Base

| Base | Points | Max digits | fast_io geo ns | std geo ns | fast/std geo | Winner points | Best ratio | Worst ratio |
|---:|---:|---:|---:|---:|---:|:---|---:|---:|
| 2 | 64 | 64 | 3.287 | 7.031 | 0.467x | fast_io 62 / std 2 | 0.283x at d57 | 1.096x at d4 |
| 3 | 41 | 41 | 8.607 | 28.398 | 0.303x | fast_io 41 / std 0 | 0.177x at d39 | 0.944x at d3 |
| 4 | 32 | 32 | 3.510 | 23.029 | 0.152x | fast_io 32 / std 0 | 0.048x at d31 | 0.826x at d3 |
| 5 | 28 | 28 | 6.511 | 18.394 | 0.354x | fast_io 27 / std 1 | 0.171x at d27 | 1.038x at d3 |
| 6 | 25 | 25 | 6.255 | 16.320 | 0.383x | fast_io 24 / std 1 | 0.177x at d25 | 1.015x at d3 |
| 7 | 23 | 23 | 6.150 | 15.014 | 0.410x | fast_io 22 / std 1 | 0.192x at d23 | 1.039x at d4 |
| 8 | 22 | 22 | 3.106 | 4.840 | 0.642x | fast_io 21 / std 1 | 0.431x at d21 | 1.037x at d3 |
| 9 | 21 | 21 | 5.639 | 13.141 | 0.429x | fast_io 21 / std 0 | 0.183x at d21 | 0.999x at d3 |
| 10 | 20 | 20 | 3.020 | 5.491 | 0.550x | fast_io 20 / std 0 | 0.485x at d20 | 0.687x at d4 |
| 11 | 19 | 19 | 6.036 | 12.065 | 0.500x | fast_io 18 / std 1 | 0.217x at d19 | 1.180x at d6 |
| 12 | 18 | 18 | 5.565 | 11.675 | 0.477x | fast_io 17 / std 1 | 0.227x at d17 | 1.039x at d3 |
| 13 | 18 | 18 | 5.165 | 11.937 | 0.433x | fast_io 17 / std 1 | 0.215x at d16 | 1.017x at d4 |
| 14 | 17 | 17 | 5.380 | 11.338 | 0.475x | fast_io 17 / std 0 | 0.227x at d16 | 1.000x at d4 |
| 15 | 17 | 17 | 5.537 | 11.119 | 0.498x | fast_io 17 / std 0 | 0.245x at d17 | 0.998x at d3 |
| 16 | 16 | 16 | 3.365 | 4.284 | 0.785x | fast_io 14 / std 2 | 0.563x at d1 | 1.103x at d4 |
| 17 | 16 | 16 | 4.771 | 10.729 | 0.445x | fast_io 16 / std 0 | 0.230x at d15 | 0.999x at d3 |
| 18 | 16 | 16 | 5.084 | 10.765 | 0.472x | fast_io 16 / std 0 | 0.249x at d15 | 0.986x at d3 |
| 19 | 16 | 16 | 4.937 | 10.523 | 0.469x | fast_io 16 / std 0 | 0.233x at d15 | 0.941x at d4 |
| 20 | 15 | 15 | 5.018 | 9.856 | 0.509x | fast_io 14 / std 1 | 0.257x at d15 | 1.031x at d3 |
| 21 | 15 | 15 | 4.786 | 9.857 | 0.486x | fast_io 15 / std 0 | 0.232x at d15 | 0.986x at d3 |
| 22 | 15 | 15 | 4.946 | 9.691 | 0.510x | fast_io 15 / std 0 | 0.226x at d15 | 0.998x at d3 |
| 23 | 15 | 15 | 4.642 | 9.513 | 0.488x | fast_io 14 / std 1 | 0.224x at d15 | 1.007x at d4 |
| 24 | 14 | 14 | 4.987 | 9.294 | 0.537x | fast_io 14 / std 0 | 0.288x at d13 | 0.997x at d3 |
| 25 | 14 | 14 | 4.785 | 9.760 | 0.490x | fast_io 14 / std 0 | 0.256x at d11 | 0.958x at d3 |
| 26 | 14 | 14 | 4.511 | 9.713 | 0.464x | fast_io 13 / std 1 | 0.220x at d12 | 1.005x at d3 |
| 27 | 14 | 14 | 4.469 | 9.543 | 0.468x | fast_io 13 / std 1 | 0.240x at d13 | 1.052x at d3 |
| 28 | 14 | 14 | 4.610 | 9.452 | 0.488x | fast_io 14 / std 0 | 0.246x at d13 | 0.959x at d3 |
| 29 | 14 | 14 | 4.786 | 9.406 | 0.509x | fast_io 14 / std 0 | 0.266x at d13 | 0.968x at d4 |
| 30 | 14 | 14 | 4.877 | 9.335 | 0.522x | fast_io 13 / std 1 | 0.251x at d13 | 1.002x at d3 |
| 31 | 13 | 13 | 4.389 | 8.459 | 0.519x | fast_io 13 / std 0 | 0.283x at d13 | 0.971x at d4 |
| 32 | 13 | 13 | 3.132 | 8.508 | 0.368x | fast_io 13 / std 0 | 0.142x at d13 | 0.825x at d4 |
| 33 | 13 | 13 | 4.294 | 8.503 | 0.505x | fast_io 13 / std 0 | 0.233x at d13 | 0.988x at d3 |
| 34 | 13 | 13 | 4.265 | 8.718 | 0.489x | fast_io 13 / std 0 | 0.231x at d13 | 0.883x at d3 |
| 35 | 13 | 13 | 4.166 | 8.749 | 0.476x | fast_io 13 / std 0 | 0.220x at d13 | 0.930x at d4 |
| 36 | 13 | 13 | 4.626 | 8.483 | 0.545x | fast_io 13 / std 0 | 0.229x at d13 | 0.978x at d3 |

## Rerun Decimal And JEAIII Results

| Digits | fast_io public ns | fast_io direct jeaiii ns | std ns | upstream jeaiii ns | public/std | direct/upstream | public/upstream |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 1.428 | 0.976 | 2.615 | 0.927 | 0.546x | 1.053x | 1.541x |
| 2 | 1.329 | 0.977 | 2.510 | 0.976 | 0.530x | 1.001x | 1.362x |
| 3 | 1.910 | 1.232 | 2.926 | 1.267 | 0.653x | 0.973x | 1.508x |
| 4 | 2.350 | 1.487 | 3.421 | 1.458 | 0.687x | 1.020x | 1.612x |
| 5 | 2.289 | 1.627 | 3.874 | 1.503 | 0.591x | 1.082x | 1.523x |
| 6 | 2.455 | 1.655 | 4.014 | 1.460 | 0.612x | 1.133x | 1.681x |
| 7 | 2.604 | 1.879 | 4.503 | 1.740 | 0.578x | 1.080x | 1.496x |
| 8 | 2.818 | 1.949 | 4.850 | 1.772 | 0.581x | 1.100x | 1.591x |
| 9 | 2.976 | 2.044 | 5.559 | 2.213 | 0.535x | 0.924x | 1.345x |
| 10 | 3.042 | 2.511 | 5.547 | 3.273 | 0.548x | 0.767x | 0.929x |
| 11 | 3.605 | 3.176 | 6.149 | 3.155 | 0.586x | 1.007x | 1.143x |
| 12 | 3.568 | 3.169 | 6.393 | 3.150 | 0.558x | 1.006x | 1.133x |
| 13 | 3.799 | 3.396 | 7.103 | 3.219 | 0.535x | 1.055x | 1.180x |
| 14 | 3.792 | 3.404 | 7.190 | 3.232 | 0.527x | 1.053x | 1.173x |
| 15 | 3.928 | 3.431 | 7.816 | 3.353 | 0.503x | 1.023x | 1.171x |
| 16 | 3.980 | 3.427 | 7.791 | 3.353 | 0.511x | 1.022x | 1.187x |
| 17 | 4.398 | 4.102 | 8.865 | 4.209 | 0.496x | 0.975x | 1.045x |
| 18 | 4.421 | 4.096 | 8.818 | 5.565 | 0.501x | 0.736x | 0.794x |
| 19 | 4.705 | 4.472 | 9.695 | 4.656 | 0.485x | 0.960x | 1.011x |
| 20 | 4.732 | 4.483 | 9.757 | 4.663 | 0.485x | 0.961x | 1.015x |

## Rerun Remaining fast_io/std Losses

| Base | Digits | fast_io ns | std ns | ratio |
|---:|---:|---:|---:|---:|
| 11 | 6 | 8.137 | 6.899 | 1.180x |
| 16 | 4 | 5.243 | 4.751 | 1.103x |
| 2 | 4 | 3.073 | 2.805 | 1.096x |
| 2 | 3 | 2.628 | 2.438 | 1.078x |
| 16 | 3 | 2.659 | 2.496 | 1.065x |
| 27 | 3 | 3.428 | 3.258 | 1.052x |
| 12 | 3 | 3.420 | 3.290 | 1.039x |
| 7 | 4 | 3.871 | 3.727 | 1.039x |
| 5 | 3 | 3.296 | 3.175 | 1.038x |
| 8 | 3 | 3.033 | 2.926 | 1.037x |
| 20 | 3 | 3.345 | 3.244 | 1.031x |
| 13 | 4 | 4.147 | 4.079 | 1.017x |
| 6 | 3 | 3.253 | 3.203 | 1.015x |
| 23 | 4 | 3.975 | 3.946 | 1.007x |
| 26 | 3 | 3.360 | 3.342 | 1.005x |
| 30 | 3 | 3.476 | 3.471 | 1.002x |

## Rerun Complete fast_io/std Matrix

Each entry is `digit-length:fast_io-ns/std-ns=ratio`; no valid `uint64_t` length is omitted.

| Base | Paired fast_io/std timings |
|---:|:---|
| 2 | d1:1.528/2.875=0.531x d2:1.992/2.515=0.792x d3:2.628/2.438=1.078x d4:3.073/2.805=1.096x d5:2.714/3.022=0.898x d6:2.908/3.079=0.944x d7:3.068/3.472=0.884x d8:3.307/3.869=0.855x d9:2.713/3.595=0.754x d10:2.904/3.745=0.776x d11:2.917/3.912=0.746x d12:3.037/4.249=0.715x d13:2.836/4.544=0.624x d14:3.084/4.448=0.693x d15:3.333/5.007=0.666x d16:4.161/4.959=0.839x d17:2.721/4.923=0.553x d18:2.923/5.142=0.569x d19:2.977/5.186=0.574x d20:3.376/5.669=0.595x d21:2.795/5.800=0.482x d22:3.120/5.938=0.525x d23:3.261/6.254=0.521x d24:3.684/7.047=0.523x d25:2.900/6.278=0.462x d26:3.135/6.544=0.479x d27:3.228/6.753=0.478x d28:3.682/6.914=0.533x d29:3.107/7.080=0.439x d30:3.301/7.366=0.448x d31:3.551/7.677=0.463x d32:3.743/7.871=0.476x d33:2.942/7.583=0.388x d34:3.127/7.878=0.397x d35:3.305/8.014=0.412x d36:3.653/8.282=0.441x d37:3.093/8.490=0.364x d38:3.307/8.823=0.375x d39:3.662/8.919=0.411x d40:3.775/9.247=0.408x d41:3.084/8.845=0.349x d42:3.318/9.136=0.363x d43:3.431/9.444=0.363x d44:3.842/9.675=0.397x d45:3.328/9.871=0.337x d46:3.412/10.090=0.338x d47:3.618/10.386=0.348x d48:4.062/10.622=0.382x d49:3.122/10.365=0.301x d50:3.316/10.658=0.311x d51:3.773/11.086=0.340x d52:3.968/11.315=0.351x d53:3.392/11.571=0.293x d54:3.812/11.562=0.330x d55:3.966/12.043=0.329x d56:4.086/12.035=0.340x d57:3.331/11.769=0.283x d58:3.703/12.015=0.308x d59:3.890/12.249=0.318x d60:4.120/12.692=0.325x d61:3.663/12.671=0.289x d62:3.891/12.937=0.301x d63:4.155/13.033=0.319x d64:4.267/13.645=0.313x |
| 3 | d1:1.551/3.020=0.514x d2:2.126/2.901=0.733x d3:3.284/3.479=0.944x d4:3.667/4.000=0.917x d5:4.053/5.811=0.697x d6:4.464/6.970=0.640x d7:5.008/8.358=0.599x d8:5.156/9.469=0.544x d9:5.767/11.729=0.492x d10:6.024/12.567=0.479x d11:6.242/13.783=0.453x d12:6.494/14.834=0.438x d13:6.777/17.778=0.381x d14:7.298/19.555=0.373x d15:7.480/21.607=0.346x d16:7.422/23.730=0.313x d17:8.169/27.256=0.300x d18:8.444/28.687=0.294x d19:8.504/29.904=0.284x d20:8.443/31.615=0.267x d21:9.159/36.028=0.254x d22:9.798/39.629=0.247x d23:10.043/42.899=0.234x d24:10.326/45.730=0.226x d25:11.000/50.778=0.217x d26:11.553/53.733=0.215x d27:11.886/56.419=0.211x d28:12.083/58.807=0.205x d29:12.840/64.217=0.200x d30:13.073/66.492=0.197x d31:13.345/69.795=0.191x d32:14.480/73.746=0.196x d33:15.014/79.008=0.190x d34:15.523/80.796=0.192x d35:15.967/84.499=0.189x d36:16.238/88.101=0.184x d37:17.343/91.511=0.190x d38:16.966/94.336=0.180x d39:17.248/97.697=0.177x d40:18.342/101.488=0.181x d41:19.331/109.213=0.177x |
| 4 | d1:1.601/3.071=0.521x d2:2.211/3.206=0.689x d3:2.827/3.423=0.826x d4:3.035/4.129=0.735x d5:6.087/10.348=0.588x d6:6.365/11.402=0.558x d7:6.549/13.130=0.499x d8:6.977/14.997=0.465x d9:3.042/12.459=0.244x d10:3.246/13.661=0.238x d11:3.261/15.090=0.216x d12:3.565/16.042=0.222x d13:3.246/19.772=0.164x d14:3.512/22.246=0.158x d15:3.528/24.120=0.146x d16:3.644/26.801=0.136x d17:3.301/30.748=0.107x d18:3.208/29.962=0.107x d19:3.250/32.461=0.100x d20:3.341/34.298=0.097x d21:3.039/39.659=0.077x d22:3.247/42.329=0.077x d23:3.447/45.245=0.076x d24:3.414/48.430=0.070x d25:3.147/53.980=0.058x d26:3.573/57.444=0.062x d27:3.465/58.832=0.059x d28:3.653/60.830=0.060x d29:3.421/66.517=0.051x d30:3.515/69.301=0.051x d31:3.480/72.041=0.048x d32:3.900/74.316=0.052x |
| 5 | d1:1.416/2.622=0.540x d2:1.986/2.899=0.685x d3:3.296/3.175=1.038x d4:3.573/4.005=0.892x d5:4.057/5.887=0.689x d6:6.432/6.921=0.929x d7:5.705/8.140=0.701x d8:5.370/9.446=0.568x d9:6.002/11.453=0.524x d10:7.739/12.396=0.624x d11:6.222/13.791=0.451x d12:6.341/14.787=0.429x d13:6.522/17.714=0.368x d14:7.915/21.524=0.368x d15:7.129/27.460=0.260x d16:7.898/27.632=0.286x d17:7.609/31.765=0.240x d18:8.007/32.916=0.243x d19:8.058/37.483=0.215x d20:8.598/39.211=0.219x d21:8.998/43.461=0.207x d22:9.765/45.924=0.213x d23:9.668/49.084=0.197x d24:9.772/51.973=0.188x d25:9.809/56.286=0.174x d26:10.524/58.995=0.178x d27:10.725/62.695=0.171x d28:11.077/62.292=0.178x |
| 6 | d1:1.577/2.654=0.594x d2:1.939/2.947=0.658x d3:3.253/3.203=1.015x d4:3.523/4.520=0.779x d5:4.800/6.173=0.778x d6:4.657/7.173=0.649x d7:6.623/8.391=0.789x d8:5.921/9.334=0.634x d9:6.246/11.544=0.541x d10:6.248/12.533=0.499x d11:6.292/13.625=0.462x d12:6.858/14.909=0.460x d13:7.203/20.247=0.356x d14:7.991/23.687=0.337x d15:8.204/26.391=0.311x d16:7.326/28.512=0.257x d17:7.635/31.865=0.240x d18:8.016/34.598=0.232x d19:8.488/36.647=0.232x d20:8.842/39.257=0.225x d21:9.439/43.726=0.216x d22:10.056/46.833=0.215x d23:9.873/49.652=0.199x d24:10.017/52.705=0.190x d25:10.420/58.748=0.177x |
| 7 | d1:1.419/2.457=0.577x d2:1.939/2.749=0.705x d3:3.172/3.257=0.974x d4:3.871/3.727=1.039x d5:4.506/5.760=0.782x d6:5.600/6.828=0.820x d7:5.943/8.289=0.717x d8:5.085/9.469=0.537x d9:6.515/11.629=0.560x d10:6.155/12.583=0.489x d11:6.120/13.745=0.445x d12:6.464/16.487=0.392x d13:6.760/27.865=0.243x d14:15.180/32.590=0.466x d15:7.977/27.036=0.295x d16:7.912/29.278=0.270x d17:8.834/32.660=0.271x d18:8.321/35.995=0.231x d19:8.509/37.711=0.226x d20:8.794/39.448=0.223x d21:9.337/43.581=0.214x d22:10.030/46.764=0.214x d23:10.207/53.278=0.192x |
| 8 | d1:1.563/2.921=0.535x d2:1.939/2.504=0.774x d3:3.033/2.926=1.037x d4:2.933/3.039=0.965x d5:2.770/3.257=0.851x d6:2.998/3.510=0.854x d7:3.319/3.832=0.866x d8:3.136/4.162=0.753x d9:3.092/4.473=0.691x d10:3.243/4.539=0.714x d11:3.533/5.036=0.701x d12:3.351/5.092=0.658x d13:3.237/5.626=0.575x d14:3.438/5.628=0.611x d15:3.743/5.974=0.627x d16:3.529/6.372=0.554x d17:3.117/6.751=0.462x d18:3.427/6.890=0.497x d19:3.661/7.373=0.497x d20:3.510/7.331=0.479x d21:3.328/7.714=0.431x d22:3.616/8.090=0.447x |
| 9 | d1:1.551/2.596=0.598x d2:2.125/2.902=0.732x d3:3.474/3.479=0.999x d4:3.882/4.040=0.961x d5:4.445/5.980=0.743x d6:5.695/6.968=0.817x d7:5.630/8.342=0.675x d8:5.380/9.517=0.565x d9:5.892/11.567=0.509x d10:5.914/12.399=0.477x d11:5.771/15.095=0.382x d12:6.452/16.978=0.380x d13:7.072/20.544=0.344x d14:7.730/22.753=0.340x d15:7.708/25.642=0.301x d16:7.400/28.592=0.259x d17:7.890/31.781=0.248x d18:8.290/33.648=0.246x d19:8.507/35.870=0.237x d20:8.923/38.529=0.232x d21:8.686/47.533=0.183x |
| 10 | d1:1.428/2.615=0.546x d2:1.329/2.510=0.530x d3:1.910/2.926=0.653x d4:2.350/3.421=0.687x d5:2.289/3.874=0.591x d6:2.455/4.014=0.612x d7:2.604/4.503=0.578x d8:2.818/4.850=0.581x d9:2.976/5.559=0.535x d10:3.042/5.547=0.548x d11:3.605/6.149=0.586x d12:3.568/6.393=0.558x d13:3.799/7.103=0.535x d14:3.792/7.190=0.527x d15:3.928/7.816=0.503x d16:3.980/7.791=0.511x d17:4.398/8.865=0.496x d18:4.421/8.818=0.501x d19:4.705/9.695=0.485x d20:4.732/9.757=0.485x |
| 11 | d1:1.553/2.654=0.585x d2:2.125/2.902=0.732x d3:3.476/3.479=0.999x d4:3.875/4.008=0.967x d5:4.438/5.831=0.761x d6:8.137/6.899=1.180x d7:7.493/8.205=0.913x d8:6.573/9.477=0.694x d9:6.679/11.531=0.579x d10:7.434/14.087=0.528x d11:7.525/16.217=0.464x d12:6.814/18.243=0.374x d13:7.457/21.361=0.349x d14:8.165/24.317=0.336x d15:8.445/27.215=0.310x d16:8.474/29.696=0.285x d17:8.834/33.380=0.265x d18:9.279/35.720=0.260x d19:9.493/43.840=0.217x |
| 12 | d1:1.584/2.542=0.623x d2:2.113/3.092=0.683x d3:3.420/3.290=1.039x d4:3.543/4.246=0.835x d5:4.423/6.009=0.736x d6:5.596/7.168=0.781x d7:6.842/8.447=0.810x d8:6.700/9.565=0.700x d9:6.296/12.366=0.509x d10:7.479/15.577=0.480x d11:6.808/17.302=0.393x d12:7.080/19.641=0.360x d13:7.531/22.028=0.342x d14:8.349/26.436=0.316x d15:7.877/29.050=0.271x d16:7.576/31.390=0.241x d17:7.835/34.562=0.227x d18:8.532/35.399=0.241x |
| 13 | d1:1.521/2.411=0.631x d2:1.939/2.726=0.711x d3:3.402/3.694=0.921x d4:4.147/4.079=1.017x d5:4.359/5.946=0.733x d6:5.281/7.114=0.742x d7:6.309/8.404=0.751x d8:5.261/9.689=0.543x d9:5.817/13.171=0.442x d10:5.924/17.213=0.344x d11:5.954/18.969=0.314x d12:6.420/20.526=0.313x d13:6.850/22.608=0.303x d14:7.327/27.686=0.265x d15:7.100/31.755=0.224x d16:7.254/33.774=0.215x d17:7.762/34.341=0.226x d18:8.137/34.875=0.233x |
| 14 | d1:1.442/2.360=0.611x d2:2.015/2.872=0.702x d3:3.227/4.023=0.802x d4:4.244/4.246=1.000x d5:4.246/6.010=0.706x d6:4.471/7.181=0.623x d7:6.946/8.378=0.829x d8:5.770/9.569=0.603x d9:5.913/13.058=0.453x d10:5.797/15.576=0.372x d11:6.062/17.518=0.346x d12:6.826/19.273=0.354x d13:6.834/22.914=0.298x d14:7.874/26.399=0.298x d15:8.029/30.723=0.261x d16:8.915/39.239=0.227x d17:16.026/42.143=0.380x |
| 15 | d1:1.656/2.577=0.643x d2:2.211/3.020=0.732x d3:3.614/3.620=0.998x d4:4.032/4.185=0.963x d5:4.416/6.047=0.730x d6:6.202/7.245=0.856x d7:5.868/8.686=0.676x d8:5.681/9.988=0.569x d9:7.118/13.937=0.511x d10:6.646/15.960=0.416x d11:6.772/18.005=0.376x d12:7.191/19.992=0.360x d13:8.136/22.939=0.355x d14:8.913/25.766=0.346x d15:8.218/28.681=0.287x d16:7.812/29.536=0.264x d17:8.268/33.788=0.245x |
| 16 | d1:1.565/2.781=0.563x d2:1.985/2.128=0.933x d3:2.659/2.496=1.065x d4:5.243/4.751=1.103x d5:3.930/4.138=0.950x d6:3.830/3.982=0.962x d7:3.526/4.301=0.820x d8:3.881/4.358=0.890x d9:3.317/4.541=0.730x d10:3.705/4.590=0.807x d11:3.386/5.198=0.651x d12:3.714/5.010=0.741x d13:3.461/5.366=0.645x d14:3.895/5.594=0.696x d15:3.649/5.916=0.617x d16:3.975/6.172=0.644x |
| 17 | d1:1.553/2.453=0.633x d2:2.126/2.902=0.733x d3:3.476/3.479=0.999x d4:3.669/4.016=0.914x d5:4.055/5.828=0.696x d6:4.253/6.911=0.615x d7:4.439/8.338=0.532x d8:5.437/10.718=0.507x d9:5.810/15.652=0.371x d10:5.933/18.059=0.329x d11:6.009/20.316=0.296x d12:6.388/19.943=0.320x d13:7.553/27.036=0.279x d14:7.544/30.135=0.250x d15:7.411/32.200=0.230x d16:7.355/28.673=0.257x |
| 18 | d1:1.438/2.178=0.660x d2:1.939/2.647=0.732x d3:3.290/3.337=0.986x d4:3.606/4.551=0.792x d5:4.487/6.328=0.709x d6:5.765/7.211=0.799x d7:6.237/8.498=0.734x d8:5.556/10.999=0.505x d9:5.880/15.838=0.371x d10:7.038/17.976=0.392x d11:6.306/20.251=0.311x d12:6.883/20.479=0.336x d13:7.257/26.811=0.271x d14:7.990/29.644=0.270x d15:7.953/31.948=0.249x d16:8.111/29.063=0.279x |
| 19 | d1:1.459/2.072=0.704x d2:1.966/2.697=0.729x d3:3.441/3.692=0.932x d4:3.893/4.138=0.941x d5:4.346/5.913=0.735x d6:4.587/7.143=0.642x d7:5.966/8.599=0.694x d8:5.536/10.976=0.504x d9:6.290/15.324=0.410x d10:6.300/17.154=0.367x d11:6.293/19.250=0.327x d12:6.656/20.690=0.322x d13:7.000/24.854=0.282x d14:7.311/27.278=0.268x d15:7.421/31.805=0.233x d16:7.821/29.419=0.266x |
| 20 | d1:1.704/2.159=0.789x d2:2.005/2.647=0.757x d3:3.345/3.244=1.031x d4:3.485/3.772=0.924x d5:7.055/8.230=0.857x d6:5.580/8.597=0.649x d7:5.804/9.413=0.617x d8:6.289/11.339=0.555x d9:5.709/14.437=0.395x d10:6.111/15.812=0.386x d11:5.906/17.664=0.334x d12:6.853/19.654=0.349x d13:6.426/24.321=0.264x d14:7.916/25.894=0.306x d15:7.830/30.498=0.257x |
| 21 | d1:1.648/2.121=0.777x d2:2.032/2.668=0.762x d3:3.215/3.261=0.986x d4:4.893/5.881=0.832x d5:4.790/6.550=0.731x d6:4.723/7.531=0.627x d7:4.661/8.541=0.546x d8:5.207/10.756=0.484x d9:5.500/14.088=0.390x d10:6.027/15.769=0.382x d11:5.783/17.475=0.331x d12:6.639/21.075=0.315x d13:7.131/23.782=0.300x d14:7.878/25.518=0.309x d15:7.663/33.023=0.232x |
| 22 | d1:1.474/2.320=0.635x d2:2.112/2.900=0.728x d3:3.225/3.231=0.998x d4:3.736/4.520=0.827x d5:4.798/6.071=0.790x d6:6.353/7.243=0.877x d7:6.277/8.348=0.752x d8:5.229/10.791=0.485x d9:5.985/13.989=0.428x d10:6.363/15.827=0.402x d11:5.945/17.329=0.343x d12:6.551/19.984=0.328x d13:7.725/23.255=0.332x d14:7.765/26.139=0.297x d15:7.760/34.414=0.226x |
| 23 | d1:1.488/2.114=0.704x d2:1.939/2.838=0.683x d3:3.295/3.768=0.874x d4:3.975/3.946=1.007x d5:4.052/5.828=0.695x d6:4.332/6.968=0.622x d7:4.440/8.394=0.529x d8:5.407/10.856=0.498x d9:6.199/13.928=0.445x d10:6.087/15.819=0.385x d11:5.998/17.561=0.342x d12:6.941/19.917=0.348x d13:6.622/22.683=0.292x d14:8.116/24.672=0.329x d15:7.443/33.197=0.224x |
| 24 | d1:1.523/2.242=0.680x d2:2.124/2.882=0.737x d3:3.274/3.283=0.997x d4:4.242/6.307=0.673x d5:6.629/7.400=0.896x d6:6.694/7.890=0.848x d7:7.358/8.932=0.824x d8:6.104/11.127=0.549x d9:5.585/13.875=0.403x d10:5.812/15.903=0.365x d11:5.840/17.382=0.336x d12:6.350/20.559=0.309x d13:6.851/23.753=0.288x d14:7.969/24.678=0.323x |
| 25 | d1:1.547/2.193=0.705x d2:1.971/2.714=0.726x d3:3.307/3.451=0.958x d4:4.058/4.275=0.949x d5:4.053/5.835=0.695x d6:4.277/6.971=0.613x d7:4.470/9.346=0.478x d8:5.307/12.906=0.411x d9:5.751/16.345=0.352x d10:6.760/18.594=0.364x d11:6.413/25.090=0.256x d12:14.067/30.813=0.457x d13:7.561/28.173=0.268x d14:7.397/24.938=0.297x |
| 26 | d1:1.415/2.177=0.650x d2:2.125/2.675=0.794x d3:3.360/3.342=1.005x d4:3.602/4.429=0.813x d5:4.369/6.285=0.695x d6:4.658/7.109=0.655x d7:6.401/9.675=0.662x d8:5.194/13.894=0.374x d9:6.269/18.043=0.347x d10:5.686/20.151=0.282x d11:5.849/19.266=0.304x d12:5.701/25.925=0.220x d13:6.607/28.839=0.229x d14:7.473/24.525=0.305x |
| 27 | d1:1.635/2.122=0.770x d2:2.124/2.901=0.732x d3:3.428/3.258=1.052x d4:3.629/4.252=0.854x d5:4.084/5.790=0.705x d6:4.208/6.941=0.606x d7:4.458/9.794=0.455x d8:5.626/13.662=0.412x d9:6.037/17.069=0.354x d10:5.719/19.684=0.291x d11:5.578/19.428=0.287x d12:6.446/24.536=0.263x d13:6.888/28.719=0.240x d14:7.965/24.626=0.323x |
| 28 | d1:1.756/2.087=0.842x d2:1.971/2.701=0.730x d3:3.303/3.445=0.959x d4:3.672/4.470=0.821x d5:4.156/5.920=0.702x d6:4.627/7.082=0.653x d7:6.118/9.823=0.623x d8:5.639/13.226=0.426x d9:6.242/16.466=0.379x d10:5.947/18.733=0.317x d11:6.188/19.157=0.323x d12:6.188/23.988=0.258x d13:6.722/27.380=0.246x d14:7.366/24.761=0.297x |
| 29 | d1:1.553/2.156=0.720x d2:2.126/2.783=0.764x d3:3.524/3.696=0.954x d4:4.378/4.521=0.968x d5:4.272/5.801=0.736x d6:4.970/6.960=0.714x d7:5.796/9.751=0.594x d8:5.682/12.807=0.444x d9:6.257/16.199=0.386x d10:6.122/18.175=0.337x d11:6.992/19.285=0.363x d12:6.412/22.610=0.284x d13:7.011/26.384=0.266x d14:7.711/24.766=0.311x |
| 30 | d1:1.554/2.068=0.751x d2:2.103/2.901=0.725x d3:3.476/3.471=1.002x d4:3.596/4.436=0.811x d5:4.054/5.856=0.692x d6:5.685/6.997=0.813x d7:6.332/9.769=0.648x d8:5.945/12.452=0.477x d9:7.392/15.730=0.470x d10:6.979/17.778=0.393x d11:7.286/19.151=0.380x d12:6.238/22.471=0.278x d13:6.838/27.245=0.251x d14:7.580/25.283=0.300x |
| 31 | d1:1.620/2.059=0.787x d2:2.124/2.801=0.759x d3:3.369/3.480=0.968x d4:3.895/4.009=0.971x d5:4.035/5.817=0.694x d6:4.288/6.946=0.617x d7:4.439/9.680=0.459x d8:5.587/12.188=0.458x d9:5.667/15.635=0.362x d10:6.122/17.478=0.350x d11:6.052/18.859=0.321x d12:7.463/21.940=0.340x d13:7.394/26.173=0.283x |
| 32 | d1:1.417/2.131=0.665x d2:2.105/2.903=0.725x d3:2.771/3.478=0.797x d4:3.338/4.046=0.825x d5:3.337/5.858=0.570x d6:3.533/6.974=0.507x d7:3.354/9.741=0.344x d8:3.742/12.162=0.308x d9:3.346/15.417=0.217x d10:3.765/17.214=0.219x d11:3.596/18.985=0.189x d12:4.036/21.868=0.185x d13:3.741/26.374=0.142x |
| 33 | d1:1.539/2.204=0.698x d2:1.940/2.901=0.669x d3:3.438/3.480=0.988x d4:3.601/4.012=0.898x d5:4.096/6.028=0.680x d6:4.248/6.943=0.612x d7:4.448/9.587=0.464x d8:6.134/11.883=0.516x d9:6.144/15.114=0.407x d10:5.995/16.901=0.355x d11:6.497/18.875=0.344x d12:6.241/21.387=0.292x d13:6.439/27.670=0.233x |
| 34 | d1:1.552/2.203=0.705x d2:2.030/2.749=0.739x d3:3.072/3.479=0.883x d4:3.303/4.344=0.761x d5:4.896/7.613=0.643x d6:4.901/7.568=0.648x d7:4.507/10.039=0.449x d8:5.512/11.915=0.463x d9:5.692/14.894=0.382x d10:5.990/16.493=0.363x d11:5.681/18.590=0.306x d12:6.480/20.887=0.310x d13:6.465/27.946=0.231x |
| 35 | d1:1.525/2.120=0.719x d2:2.080/2.900=0.717x d3:3.207/3.478=0.922x d4:3.597/3.866=0.930x d5:4.392/6.857=0.640x d6:4.447/7.167=0.621x d7:4.439/13.320=0.333x d8:5.276/11.839=0.446x d9:5.200/14.883=0.349x d10:5.583/16.393=0.341x d11:5.743/18.849=0.305x d12:6.396/20.563=0.311x d13:6.376/29.033=0.220x |
| 36 | d1:1.412/2.050=0.689x d2:2.086/2.901=0.719x d3:3.261/3.332=0.978x d4:3.520/4.579=0.769x d5:5.141/6.320=0.813x d6:5.609/7.249=0.774x d7:6.287/9.693=0.649x d8:6.400/11.574=0.553x d9:6.256/14.358=0.436x d10:6.295/15.992=0.394x d11:6.285/18.439=0.341x d12:6.771/20.219=0.335x d13:6.764/29.498=0.229x |

Correctness preflight: `ok` for every timed value before measurement; no `std::errc` mismatch, length mismatch, or byte mismatch was observed.

## 2026-07-16 all-type, all-base compiler matrix

The 2026-07-16 production matrix uses the same value population as the input
benchmark: unsigned and signed 8-, 16-, 32-, 64-, and 128-bit integers, every
valid length in bases 2 through 36, and `char`, `wchar_t`, `char8_t`,
`char16_t`, and `char32_t`.  The public `fast_io::to_chars` and
`std::to_chars` comparison is available for `char`; the other character types
exercise the reserve-print core that is used by fast_io formatting.

The historical x86-64 host is an Intel Core i9-14900HX.  Each benchmark process was pinned
to P-core logical CPU 4 and all builds and runs were serialized.  Compilers are
GCC 13.4, GCC 14.3, GCC 15.2, a GCC 16 development snapshot, Clang 22,
upstream Clang 23, and the fast_io Clang 23 toolchain, using C++20, `-O3`, and
`-march=native`.  Ratios above one favor fast_io.

| Compiler | public/core | runtime/core | std/public | direct JEAIII/core |
|:---|---:|---:|---:|---:|
| GCC 13.4 | 1.000x | 1.354x | 1.618x | 0.996x |
| GCC 14.3 | 0.999x | 1.379x | 1.635x | 1.006x |
| GCC 15.2 | 1.004x | 1.356x | 1.669x | 0.999x |
| GCC 16 development | 1.001x | 1.334x | 1.654x | 1.001x |
| Clang 22 | 0.997x | 1.319x | 1.448x | 1.001x |
| Clang 23 upstream | 0.998x | 1.318x | 1.436x | 0.996x |
| Clang 23 fast_io | 1.003x | 1.315x | 1.427x | 1.003x |

For this historical 2026-07-16 noinline benchmark, the fixed public wrapper is
effectively the same cost as the core: all retained deviations are within
approximately one percent.  Runtime-base dispatch costs 30--36% on this
benchmark's noinline boundary because it must select among 35 complete base
specializations.  That cost is not present when the call is inlined with a
constant base.  Direct JEAIII and the fast_io decimal core are equal because
the retained decimal
implementation is JEAIII; this comparison checks wrapper and selection cost,
not two independent decimal algorithms.

The preceding x86 conclusion and the M4 table immediately below are specific
to the 2026-07-16 benchmark shape.  The 2026-07-17 production matrix below
reports a `1.1475x` fixed/core latency multiplier and supersedes them for the
frozen implementation.

The same historical benchmark shape gave similar Apple M4 ratios:

| Compiler | public/core | runtime/core | std/public | direct JEAIII/core |
|:---|---:|---:|---:|---:|
| Apple Clang 21 | 1.016x | 1.423x | 1.643x | 0.999x |
| Clang 23 | 1.018x | 1.363x | 1.546x | 1.000x |

The public API now accepts the library's 128-bit integral concept rather than
only standard-library integral types.  Power-of-two bases use the separated
128-bit printer when required, preventing truncation through a 64-bit-only
helper.  Fixed- and runtime-base public wrappers share the same checked
capacity and signed-magnitude handling.  GCC otherwise leaves a separate
public-wrapper call even for a literal base; the targeted GCC-only inline
attribute is therefore confined to this public dispatcher and is not applied
to the scanner or conversion kernels.

### Champagne--Lemire AVX-512 path

The native i9-14900HX does not expose AVX-512 IFMA and VBMI, so no
Champagne--Lemire timing is reported for this host.  The AVX-512 decimal path
was instead compiled independently with Clang 23 and GCC 15 for Sapphire
Rapids and executed with Intel SDE 10.8 in `-spr` mode.  Each compiler passed
300,080 comparisons over decimal power boundaries, signed limits, and 100,000
deterministic random 64-bit values.  The test compares the direct
Champagne--Lemire output, direct JEAIII output, `char8_t` output, and the public
`to_chars` result byte for byte.  SDE is used only for correctness; emulator
time is not presented as native throughput.

### EBCDIC output validation

GCC 13 through 16 ran the complete output matrix with IBM1047 execution and
wide-execution character sets.  Public/core ratios are 1.000x, 0.997x, 0.998x,
and 1.000x respectively; direct JEAIII/core ratios remain within about 0.8% of
one.
`char` and `wchar_t` use execution-EBCDIC values, while UTF character types use
Unicode values.  Numeric ASCII constants are confined to the explicitly ASCII
SIMD implementation, and the native EBCDIC formatter continues to obtain its
digits through the character-literal abstraction.

The correctness pass covers exact capacity and every insufficient capacity,
returned pointers, `std::errc::value_too_large`, and fast_io's stronger tested
short-buffer range-preservation behavior.  The latter is not generalized into
a C++ standard guarantee.  Signed minima, unsigned maxima, radix-power
boundaries, and all five output character types are also covered.  The same
preflight runs before every timed point, so no row whose output disagreed with
the independent reference entered the performance summary.

## Public five-character API follow-up

`fast_io::to_chars` now deduces and directly writes `char`, `wchar_t`,
`char8_t`, `char16_t`, or `char32_t`.  `basic_to_chars_result<Char>` returns the
matching pointer type, while `basic_to_chars_result<char>` remains exactly
`std::to_chars_result`.  The integer template parameter remains first for
source compatibility with explicit `to_chars<uint64_t>(...)` calls.  Decimal,
power-of-two, generic-radix, signed-magnitude, capacity, and two-digit-table
paths operate on the destination code-unit type without transcoding.

The AVX-512 Champagne--Lemire byte writer is restricted to one-byte,
non-EBCDIC code units.  Wider outputs use the generic JEAIII character template,
preventing packed byte stores from corrupting UTF-16/32 or `wchar_t` buffers.
The complete 50-combination native M4, x86-64 GCC 15, and GCC 15 IBM1047
matrices all pass, together with constexpr round trips for all five types.  The
shared public-interface production fuzzer contributes 76,388,264 ASan/UBSan
executions with no finding.

The complete 665-point `uint64_t` fixed-base public/core ratios are:

| Host/compiler | `char` | `wchar_t` | `char8_t` | `char16_t` | `char32_t` |
|:---|---:|---:|---:|---:|---:|
| Apple M4, Apple Clang 21 | 1.000x | 0.999x | 1.001x | 0.992x | 1.010x |
| i9-14900HX, GCC 15 | 1.022x | 1.010x | 1.016x | 1.007x | 1.010x |

The timing core intentionally omits capacity and sign checks; the GCC public
cost therefore includes required API semantics.  When public wrappers are
compared with the internal fixed-base helper carrying the same checks, GCC 15
emits identical sizes for all five code-unit types at bases 10 and 16.  M4
does the same: representative `char16_t` public/core pairs are 17/17
instructions at base 10 and 31/31 at base 16, with identical call counts.
Runtime-base measurements are kept separate because selecting among 35 radix
specializations is genuine runtime work, not character-template wrapping.

## Compact runtime-base dispatcher follow-up (2026-07-16)

The public runtime-base formatter previously retained 35 fixed-base template
specializations.  That made a single dynamic `uint64_t`/`char` entry occupy
roughly 92--101 KiB.  The replacement has two explicitly different compiler
outcomes:

- A literal base is recognized with `__builtin_constant_p` and still enters the
  corresponding fixed-base template.  It does not retain the compact runtime
  kernel, its digit tables, its reciprocal tables, or its power-base dispatch.
- A genuinely dynamic base enters one compact kernel.  Decimal retains JEAIII;
  bases 2, 4, 8, 16, and 32 use shift-based output; all other bases share one
  generic loop.  x86-64 uses branch-free multiply-high reciprocal division,
  while AArch64 uses its measured-faster hardware division path.

The isolated public probes for literal bases 3, 10, and 16 contain no dynamic
jump table and no `to_chars_runtime_*` symbol on either M4 or x86-64.  Clang 21
on x86-64 emits totals of 1,253, 3,565, and 1,153 bytes for those three probes,
versus 6,241 bytes for the complete dynamic entry.  On M4 the corresponding
totals are 678, 3,564, 2,336, and 4,956 bytes.  The platform difference comes
from the established fixed-base power kernels, not from leaked runtime
dispatch.

### Runtime code size

Section totals are `.text + .rodata` after function/data section splitting and
`ld -r --gc-sections`.  The Linux probe uses C++20, `-O3`, `-march=haswell`, no
exceptions, no RTTI, and no unwind tables.  Both Clang standard-library rows
have the same result because the retained implementation is self-contained.

| Compiler | Old bytes | Compact bytes | Reduction |
|:---|---:|---:|---:|
| GCC 13 | 98,201 | 7,111 | 92.8% |
| GCC 14 | 92,085 | 7,316 | 92.1% |
| GCC 15 | 91,806 | 7,247 | 92.1% |
| GCC 16 | 93,271 | 7,115 | 92.4% |
| Clang 18 | 101,646 | 6,143 | 94.0% |
| Clang 19 | 101,297 | 6,247 | 93.8% |
| Clang 20 | 101,266 | 6,241 | 93.8% |
| Clang 21 | 101,260 | 6,241 | 93.8% |

The retained x86-64 dynamic call graph contains 2,104 bytes of read-only data:
the decimal table, 704 bytes of compact binary/octal/hexadecimal grouping
tables, 36 digit code units, three 280-byte reciprocal tables, and the
compiler's small dispatch table and alignment.  M4 does not reference or emit
the x86 reciprocal tables; its linked dynamic entry contains 1,140 bytes of
constants.

GCC instantiates the fixed-base template references while compiling the
`__builtin_constant_p` dispatcher, even when the caller supplies a dynamic
base.  Consequently, a relocatable GCC object built without section GC can
still contain about 52 KiB of unreferenced COMDAT tables.  They are not
reachable from the dynamic function after optimization, and
`-ffunction-sections -fdata-sections` with linker `--gc-sections` removes them,
producing the 7.1--7.3 KiB totals above.  Clang suppresses those dead
instantiations before object emission.  Removing the GCC literal template
references made the raw object small but slowed fixed-base output by 57--90%
in tested alternatives, so that regression was rejected.

### Dynamic-base throughput

This focused benchmark prevents constant propagation with an out-of-line call
whose base is carried in the input object.  It covers every radix-power
boundary for `uint64_t`, one value on both sides where representable, and 32
deterministic random values per base.  Each reported process result is the
median of five 2,000-repetition trials; the table below takes the median of
three sequential processes.  The Linux processes were pinned to P-core 4 on an
i9-14900HX.  `std/fast_io` greater than one favors fast_io.

| Host/compiler | Group | fast_io ns | std ns | median `std/fast_io` |
|:---|:---|---:|---:|---:|
| Apple M4, Clang 20 | all bases | 10.554 | 11.814 | 1.118x |
|  | decimal | 3.699 | 4.580 | 1.235x |
|  | power of two | 7.269 | 9.777 | 1.345x |
|  | other bases | 11.755 | 12.756 | 1.091x |
| i9-14900HX, GCC 15 | all bases | 11.567 | 24.410 | 2.113x |
|  | decimal | 3.281 | 7.104 | 2.162x |
|  | power of two | 4.915 | 17.929 | 3.617x |
|  | other bases | 13.413 | 26.143 | 1.949x |
| i9-14900HX, Clang 21 | all bases | 11.128 | 26.097 | 2.357x |
|  | decimal | 2.962 | 6.363 | 2.138x |
|  | power of two | 5.960 | 15.262 | 2.561x |
|  | other bases | 12.588 | 24.959 | 1.987x |

The compact dynamic path is intentionally smaller than the former 35-kernel
runtime expansion and is correspondingly slower than that approximately
100-KiB expansion for some non-decimal values.  Fixed-base and literal-base
performance is unchanged.  The compact path was faster than the tested
standard-library dynamic path in each focused 2026-07-16 aggregate above.  This
is not a current all-matrix claim; see the production addendum.

### Assembly and scheduling audit

The M4 power loops write four binary digits or two octal/hexadecimal digits per
iteration.  LLVM-MCA 23 reports block throughputs of 2.5, 2.5, and 3.0 cycles
respectively.  The generic M4 digit loop is eight instructions and is dominated
by the modeled 13-cycle `udiv`; native A/B testing rejected the reciprocal
variant on M4.  On x86-64 the generic loop contains no `div`: one reciprocal
computes the quotient by base squared and a second splits the two-digit
remainder.  Each iteration writes two code units.  LLVM-MCA reports 5.0 cycles
block throughput and 12.4 recurrence cycles per pair on Raptor Lake, and 4.8
and 9.25 respectively on Zen 4.  The quotient recurrence, rather than
front-end width, is the limiting dependency.  The grouped x86 power loops are
1.2--1.5 cycles per iteration across those two models.

The only runtime radix selection is the five-way power-of-two check.  M4 emits
comparisons and direct branches; Clang x86 emits a 31-entry, 124-byte jump table
whose non-power entries all share one target.  No such selection appears in a
literal-base probe, so the fixed path pays neither the front-end footprint nor
the runtime branch cost.

### Historical 2026-07-16 production validation

The deterministic oracle covers all ten signed/unsigned 8-, 16-, 32-, 64-, and
128-bit types, all five character types, bases 2--36, exact buffers, every
short-buffer position, and zero through eight excess code units.  It passes the
native M4 Release and ASan+UBSan runs, GCC 13--16, Clang 18--21 with libstdc++
15 and libc++ 20, native x86-64 GCC 15, and GCC 13--16 with IBM1047 execution
characters.

That 2026-07-16 M4 mutation pass used ten separately built libFuzzer+UBSan tasks, one
per integer type.  Each task ran for 61 seconds and selected the character
type, base, capacity, and full-width value from the fuzzer input.  It executed
155,664,999 inputs with no sanitizer report or artifact.  After the x86
two-digit kernel was added, ten Clang 21 libFuzzer+ASan+UBSan tasks repeated the
same 61-second coverage sequentially on P-core 4 of the i9-14900HX and executed
another 269,585,758 inputs.  A final parallel pass bound the ten tasks one each
to E-cores 16--25.  It executed 134,239,245 more inputs with all ten normal
exits, zero artifacts, and no sanitizer diagnostic.  The combined historical count
is 559,490,002 with no crash, timeout, out-of-memory artifact, or sanitizer
finding.
