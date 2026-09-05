# Native read-completion visibility — 2026-09-05

This report supersedes the earlier write-leaf proposal and its performance
claims. Only the read-completion-only candidate F remains in the working tree.
Earlier experiments are retained as rejected evidence, not accepted speedups.
All before/after figures below compare frozen **new before this experiment**
with new after it; they must not be relabelled as official-old comparisons.
No fast_io commit or push is included.

## Retained change and protocol proof

Only two terminal selections in `operations/readimpl/basis.h` change:
`read_all_impl` and `read_all_bytes_impl` expose their native All CPO when
the normalized observer has no coherent get area. A native complete-read is
then the ordinary data plane, not an exceptional buffer-underflow path.

For the selected observer I and original interval R, the existing terminal
dispatcher satisfies HasNativeAll(I) => G(I,R) = NativeAll(I,R).
Contracting this edge preserves the named observer, one invocation, the
empty-interval observation, priority, and failure/publication order.
Observer normalization, locking, ABI-safe value transport, and all earlier
adapters remain above the contraction. The input some state machines and
buffered misses retain their original cold dispatcher.

No decay signature or parameter category changes. No reference is added, no
observer is copied, and no ABI marker, compiler-version gate, force-inline
attribute, runtime length threshold, or staging-allocation policy is added.
English source comments document the exact contraction and excluded paths.
The write basis is byte-identical to the frozen pre-experiment write basis;
its unrelated earlier working-tree changes remain intact.

## Why the scope was reduced

Correct output alone did not qualify a candidate for retention.

- B exposed native complete-write leaves. Some Clang EOF chunk-one cases
  eliminated allocation, but Linux fixed-output typed-all/23-byte transfers
  repeatedly slowed about 28%, and fragmented EOF cells about 14–22%.
- C additionally exposed native some/all reads. D restricted those leaves
  to unbuffered observers. Both retained M4 small-fragment buffer regressions
  of approximately 9–15%, so neither remains.
- E removed read-some lifting and preserved buffered cold paths. Its full
  matrices and sanitizers passed, but warmed replay confirmed a M4
  typed-EOF/chunk7/raw/257-byte regression, 263.00 -> 286.91 ns, and Linux
  Clang byte-some/chunk7/raw/4096, 2646.90 -> 2880.14 ns.
- F removes the write-side experiment entirely. Every some/EOF executable
  in the final transmit matrix is identical to its pre-change executable.
  The earlier EOF allocation-elimination figures are therefore **not**
  retained improvements, including the attractive empty fixed-output result.

The prior print N2 always-inline experiment was also rejected: the same
1028-byte outlined body merely moved into another wrapper, with identical
4312-byte text. The remaining print N2 and large-to gaps are not fixed here.

## Final transmit matrix

The fixture retains its default 4096-byte capacity and original eleven-field
result. Explicit large-capacity builds use 131074 bytes and payloads through
131073, leaving one spare output byte to avoid official-old's independently
observed exact-capacity output-loss bug. Official headers were not modified.

Both old and new use a native 131072-byte transmit request budget, not 4096;
4096 applies only to size_t no wider than 16 bits. A compile-time identity
check now verifies this premise. The primitive-count oracle uses the smaller
of input chunk and staging request, plus the final empty EOF read.

There are 56 executable cells per compiler: six typed/byte all/some/EOF
grammars, chunks 1/3/7/23, raw/fixed outputs, and eight large EOF cells around
the 64 KiB and 128 KiB boundaries. Small profiles cover 0/1/23/257/4096 bytes,
zero bound, a requested prefix, and early EOF where valid. Large profiles
cover 0/4096/65537/131072/131073.

All builds precede explicit warmups, calibration, and symmetric ABBA timings.
Samples target 40 ms, with independent inner/outer 800 ms bounds. Preflight
checks complete output bytes, progress, primitive counts, identity and a
closed-form cumulative checksum. Normalization counts are report-only.
These are finite in-memory protocol fixtures, not filesystem throughput claims.

M4 uses one compilation/benchmark child at a time, entirely under /tmp,
Clang 23, explicit SDK sysroot, -march=native, C++23, -O3 and lld.
Linux uses verified P-core 14, GCC 16 and Clang 23, -march=native, the same
source and symmetric flags; compilation is serial with a 4 GiB virtual-memory
cap and an 8 GiB available-memory guard.

| Host/compiler | Builds PASS | Measurements PASS | All checked executions PASS |
| --- | ---: | ---: | ---: |
| M4 Clang 23 | 112 | 1632 | 5025 |
| Linux GCC 16 + Clang 23 | 224 | 3264 | 10460 |

Representative typed-all, chunk-one, 4096-byte transfers (ns/call):

| Host/compiler / output | Before | After |
| --- | ---: | ---: |
| M4 Clang / fixed | 1114.16 | 95.26 |
| M4 Clang / raw | 2109.34 | 1097.80 |
| Linux Clang / fixed | 819.09 | 64.69 |
| Linux Clang / raw | 1548.09 | 808.57 |
| Linux GCC / fixed | 61.26 | 63.24 |
| Linux GCC / raw | 67.52 | 67.17 |

Typed/byte-all paired geometric-mean ratios are 0.6797/0.6941 on M4,
0.6353/0.6440 for Linux Clang, and 1.0082/1.0076 for Linux GCC.
The GCC aggregate does not establish a throughput improvement.
The raw GCC before/after executables are identical; their timing differences
are not code improvements or regressions.

Forty of 56 M4 and Linux Clang build pairs, and 48 of 56 GCC pairs, are
byte-identical. All some/EOF cases belong to that identical subset; do not
interpret their aggregate timing drift as an optimization effect.

## Replayed tradeoffs, not hidden by the mean

Every initially >5% slower group with changed executable bytes is replayed
in three additional warmed ABBA rounds. Identical executables are recorded
separately rather than assigned a fictional code regression.

M4 replay has 238/238 PASS records. All 16 persisting >5% slow groups are
zero-length exact transfers to raw outputs: most increase approximately
0.7 ns (about 1.4 -> 2.1 ns), with observed replay deltas 0.53–0.98 ns.
The initially slow nonempty one-byte fixed case replays at 1.0196.
The zero-length cost is real at this fixture's measurement floor, not a claim
of universally regression-free latency.

Linux replay has 476/476 PASS records. Clang's one-byte exact transfers
persistently cost about 1.07–1.28 ns more: raw ratios are 1.183–1.205 and
fixed-output ratios 1.138–1.154. GCC's changed zero-length fixed cases add
0.14–0.23 ns. Two byte-all/fixed/257-byte cases add 0.66–0.78 ns
(1.061–1.074); other initially slow nonempty GCC cases replay below 1.05.
These small absolute regressions remain open, not silently waived as noise.

## Compiler cost and actual code size

Sizes are sums of executable .text/.text.* or Mach-O __text sections from
llvm-size SysV output. Darwin's default size reports an aligned segment and
GNU's default text total includes read-only data; neither was used for these
final instruction-section figures. Original earlier CSVs remain intact and
received separate text-sections.csv supplements.

Observed paired geometric means, after/before:

| Compiler | Compile+link wall | Peak compiler RSS | File bytes | Text-section bytes |
| --- | ---: | ---: | ---: | ---: |
| M4 Clang | 0.9934 | 1.0010 | 0.9992 | 0.9915 |
| Linux GCC | 0.9990 | 1.0000 | 1.0000 | 0.9971 |
| Linux Clang | 1.0016 | 0.9997 | 0.9976 | 0.9968 |

No individual final transmit text or file-size pair increases. Compile-cost
figures have only one build per side per cell and are observations, not
statistical proof of equal compile time. In particular M4's largest individual
wall-time ratio is 1.3193, despite its aggregate near one.

## Why GCC improves less than Clang here

Linked pre-change GCC code already contains the complete input copy inline,
including 32-byte vmovdqu loop operations and scalar tails. In the selected
raw cells the final before/after binaries are identical.

Clang's pre-change body instead calls read_all_cold_impl, whose linked
implementation copies one byte per iteration. The retained contraction makes
the input primitive visible in the caller; Clang recognizes the loop as
memcpy. Output buffering still uses its original completion dispatch.
Malloc/free remain in these exact-transfer bodies: the gain must not be
described as allocation elimination.

This is a concrete cold-boundary and optimizer-visibility difference for this
fixture, not a universal ranking of GCC and Clang. No compiler fork was
modified and no llvm-mca/SDE output was treated as native benchmark evidence.

## Semantic, sanitizer and wider CPO checks

read_native_leaf_priority.cc covers 1536 combinations: 16 independently
admitted native typed/byte some/all masks, buffered/unbuffered observers,
four entry points, three capacities, empty/nonempty and success/failure.
Always-present scatter alternatives write distinguishable bytes. The fixture
owns mutable state, is noncopyable, checks identity and cursor progress, and
verifies failure before publication. Static assertions require the intended
buffer concept, including its bool refill hook; a malformed earlier fixture
lacking that hook was corrected, not mistaken for a library defect.

write_completion_leaf_priority.cc retains 768 independently checked cases,
including native-byte priority, retry, empty scatter behavior, exact buffer
fits and publication order. It remains useful regression coverage although
the write optimization was rejected.

Final M4 semantic validation passes 88/88 attempts: 22 tests under C++20,
C++23, independent ASan and independent fatal UBSan. Artifact:
/tmp/write_completion_validation.nhpw45lt. macOS ASan disables leak detection;
it is not LSan evidence.

The final Linux matrix passes 108/110 strict attempts. The input-priority
test passes GCC 11–16 and Clang 17–23 in both C++20/C++23 (26/26).
Fourteen tests under GCC 16 and Clang 23 pass all 28 ASan and all 28 standalone
LSan runs. Two GCC UBSan builds are stopped by -Werror=maybe-uninitialized,
not a runtime sanitizer diagnostic: transmit_byte_domain_and_zero and
transmit_decay_transport_contract. The original failures remain in
/tmp/fast_io_write_leaf.zHb1hK/read-only-leaf-final/results.csv.

Both strict diagnostics also reproduce with the frozen before headers.
Downgrading only -Wmaybe-uninitialized lets both tests compile and pass fatal
UBSan before and after (four successful diagnostic-control runs). Thus all
28 final UBSan runtime checks pass, but the strict matrix is still recorded
as 108 PASS / 2 compile failures, not rewritten as 110 strict passes.
No production zero-initialization or project-wide warning suppression was
introduced. Controls and original diagnostics are retained under
/tmp/fast_io_write_leaf.zHb1hK/read-only-leaf-warning-replay.

All 128 print/concat cells were rebuilt against the final headers: every
executable is byte-identical to its frozen pre-change counterpart. Coverage
includes eight source/concept families (including mixed borrowed packs),
packs 1/2/8/32, buffered/raw print, and std/fast string concat results.
Artifact: /tmp/read_only_cpo_identity.w03_nmog. This is code-identity evidence,
not a claim of a new output speedup or another 128 timed comparisons.

The final scan/to/protocol/transcoder run passes 60 builds, 60 warmup
executions and 120 measurements. All 29 before/new scan, to and protocol
executable pairs are byte-identical. Adapter-streaming and staged-transcoder
controls are after-only controls, not a before/after optimization comparison.
Artifact: /tmp/read_only_leaf_state_final_20260905.

Five initial timing outliers, despite identical executable bytes, range up to
1.4961. Three additional warmed ABBA rounds produce 70/70 PASS records and
ratios 0.9828–1.0051. Original timings remain intact; none is attributed to a
source regression. Replay: /tmp/read_only_state_noise.rrgf1e7u.

Earlier broader-candidate evidence is not silently relabelled as final:
the 128-cell print/concat run had 256 fresh builds and 1408 independently
audited runtime records. Its initial 176 failures were an expected-label
mistake (mixed_b versus mixed-borrowed); the original statuses and full raw
audit remain preserved. The broader read/write state run had 60 builds,
60 warmups and 120 measurements PASS, with no >5% slow paired scan/to/protocol
case. Those runs informed scope reduction but are distinct snapshots.

## Artifacts and remaining scope

Final transmit:

- M4: /tmp/write_leaf_transmit_cross.bclc3l5s
- Linux: /tmp/write_leaf_transmit_cross.0q54xhgj
- M4 replay: /tmp/exact_leaf_replay.qiu3dlzd
- Linux replay: /tmp/exact_leaf_replay.x262_0jp

Each transmit root retains source/header snapshots, command lines, compiler
costs, full raw output, a manifest, per-case paired summaries, binary hashes
and linked read-completion disassembly. Linux summary copies are under
/tmp/write_leaf_linux_results_20260905/read-only-final.

Rejected E:

- M4: /tmp/write_leaf_transmit_cross.uzt8jndd
- Linux: /tmp/write_leaf_transmit_cross.co1vc6as
- Replays: /tmp/exact_leaf_replay.zjy5a1p7 and
  /tmp/exact_leaf_replay.yshibms6

The full GCC 11–16 / Clang 17–23 **performance and compiler-cost** sweep,
wide-character transmit, every transcoder direction, all repository benchmarks,
print N2, large-to, and allocation-free empty EOF remain unfinished.
Compiler admission across all requested versions is not such a performance sweep.
The earlier independent LLVM push completed at
a9ccf30ecae04c0bdd5ae64aa39bb06d380ead00; no LLVM build was resumed here.
