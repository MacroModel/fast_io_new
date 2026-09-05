# Three-version uwvm condition benchmark

Source SHA-256: `cf6b337eeccb309afbea16dad96901c4f68fbc5c287c70fe711ee04ca4e19a12`. CPU 14; 3 rounds; 3,000,000 records per run.

Profiles: 00 = independent predicates, no timestamp; 01 = independent predicates with timestamp; 11 = shared predicate with timestamp.

All three versions must match length checksum and sampled bytes. Before/after must also match primitive calls; official calls are recorded without requiring the same strategy.

| Profile | Version | Compile s | Peak RSS MiB | Text bytes |
| --- | --- | ---: | ---: | ---: |
| 00 | before | 8.50 | 992.40 | 277554 |
| 00 | after | 4.41 | 565.84 | 13097 |
| 00 | official | 2.47 | 556.66 | 9245 |
| 01 | before | 8.79 | 1035.61 | 282869 |
| 01 | after | 4.51 | 568.37 | 16456 |
| 01 | official | 2.51 | 557.45 | 12517 |
| 11 | before | 8.81 | 1035.79 | 282805 |
| 11 | after | 4.47 | 568.13 | 16392 |
| 11 | official | 2.53 | 557.61 | 12453 |

Median ns/record; negative percentages mean the after version is faster.

| Profile | Mode | Memory | Before | After | Official | After / before | After / official |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| 00 | off | null | 20.084 | 14.729 | 18.452 | -26.7% | -20.2% |
| 00 | off | valid | 38.695 | 33.302 | 30.078 | -13.9% | +10.7% |
| 00 | on | null | 30.528 | 21.395 | 24.202 | -29.9% | -11.6% |
| 00 | on | valid | 53.628 | 42.812 | 37.074 | -20.2% | +15.5% |
| 00 | correlated | null | 27.065 | 19.544 | 23.550 | -27.8% | -17.0% |
| 00 | correlated | valid | 48.538 | 38.828 | 37.269 | -20.0% | +4.2% |
| 00 | independent | null | 68.342 | 62.362 | 36.087 | -8.8% | +72.8% |
| 00 | independent | valid | 95.773 | 76.061 | 50.770 | -20.6% | +49.8% |
| 01 | off | null | 25.610 | 20.581 | 25.701 | -19.6% | -19.9% |
| 01 | off | valid | 45.313 | 36.654 | 41.286 | -19.1% | -11.2% |
| 01 | on | null | 35.684 | 29.555 | 32.111 | -17.2% | -8.0% |
| 01 | on | valid | 60.422 | 48.107 | 47.756 | -20.4% | +0.7% |
| 01 | correlated | null | 32.132 | 25.147 | 29.818 | -21.7% | -15.7% |
| 01 | correlated | valid | 55.874 | 45.631 | 46.521 | -18.3% | -1.9% |
| 01 | independent | null | 75.500 | 65.206 | 40.694 | -13.6% | +60.2% |
| 01 | independent | valid | 102.067 | 82.185 | 66.104 | -19.5% | +24.3% |
| 11 | off | null | 26.080 | 20.943 | 25.060 | -19.7% | -16.4% |
| 11 | off | valid | 43.248 | 38.193 | 40.667 | -11.7% | -6.1% |
| 11 | on | null | 35.481 | 29.131 | 31.695 | -17.9% | -8.1% |
| 11 | on | valid | 54.955 | 49.580 | 50.418 | -9.8% | -1.7% |
| 11 | correlated | null | 32.028 | 25.297 | 29.565 | -21.0% | -14.4% |
| 11 | correlated | valid | 52.083 | 46.687 | 49.846 | -10.4% | -6.3% |
| 11 | independent | null | 36.368 | 29.409 | 32.598 | -19.1% | -9.8% |
| 11 | independent | valid | 55.212 | 48.465 | 50.676 | -12.2% | -4.4% |

Validated 72 three-version run groups.

## Reproduction and provenance

Completed on SSH host `linux` on 2026-09-05. All libraries and uwvm headers were read from independent snapshots; no original library or uwvm tree was modified for this comparison.

- Official source: `/Users/liyinan/Documents/MacroModel/src/fast_io`, commit `1a3843dd3e34d3c9b6bb8cc2dca3e698ee5ac882` (2026-07-12). The remote official snapshot contains the tracked headers and licenses from `git archive HEAD`; unrelated untracked local files were excluded.
- Before: the original fast_io_new snapshot in `baseline/include`. After: the final optimized snapshot in `final-tail/include`. Official: `official/include`.
- Artifact directory: `/home/macromodel/Documents/src/uwvm-cpo-linear.GXZozj/threeway/condition-bench-__zbsxq_`. This is the completed fresh run; earlier failed or superseded executables are excluded. Raw commands, outputs, `/usr/bin/time -v` logs, text sizes, every timing sample and medians are retained in `results.json` and the sibling directories.
- Compiler: `clang version 23.0.0git (https://github.com/llvm/llvm-project.git 4c4c1db7c69a6fda6cfa6bc6066bb09a433edc89)`. All 18 executables (nine timed and nine full-byte checks) were freshly compiled with `-std=c++26 -O3 -march=native`, the same three uwvm configuration macros, and the same benchmark source. Compilation was serial on CPU 14, with a 600-second timeout and 32-GiB virtual-memory limit per compiler process.

The official library accepts the benchmark's existing literal/null `mnp::cond` calls, real uwvm `print_memory`, hexadecimal formatting and fixed `iso8601_timestamp` directly. No adapter or alternate condition implementation was used. Its internal semantic-node/CPO strategy differs from fast_io_new, so official primitive call counts are recorded but are not required to equal those of fast_io_new. This is a synchronous memory-sink benchmark, not an end-to-end terminal or full-VM throughput result.

Run from the remote snapshot root after synchronizing the three source files into `official/`:

```sh
env LD_LIBRARY_PATH=/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/lib/x86_64-unknown-linux-gnu \
  python3 official/run_uwvm_condition_bench.py \
  snapshot baseline/include final-tail/include official/include threeway \
  --cxx /home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/bin/clang++ \
  --cpu 14 --rounds 3 --iterations 3000000 \
  --compile-timeout 600 --compile-vm-gib 32
```

## Full-byte verification

Each configuration checks all 256 masks with both null and valid real uwvm memory values: 512 records per version, 1,536 per version across all configurations. The entire byte-output files compare equal across all three versions; their common SHA-256 values and aggregate primitive calls are below. Before/after calls also match in every timed run.

| Profile | Full-byte output SHA-256 | Before calls | After calls | Official calls |
| --- | --- | ---: | ---: | ---: |
| 00 | `a050c0c11c0815e79d8894cf2d5c5683c2c10dc367e85aa2bade75a9ec25215e` | 5888 | 5888 | 11520 |
| 01 | `0d7d551a59b631a11d17393d924880f4bab6c6e2dbe48b9d21bcb5c7158c4feb` | 5888 | 5888 | 11520 |
| 11 | `7b6deaa8a7a393e2b3b78718d85b7a68c96935d45fa8f8ad7d463adf2fdac02f` | 5888 | 5888 | 13552 |

The optimized version has no median regression against its own before snapshot in these 24 cases: improvement ranges from 8.8% to 29.9%. For shared predicates plus timestamp (profile 11, closest to uwvm's shared `put_color` use), it is 1.7%–16.4% faster than the official version. The official version remains faster in several independent-predicate cases; the largest observed gap is profile 00 / independent / null, where after takes 62.362 ns versus official's 36.087 ns (+72.8%).
