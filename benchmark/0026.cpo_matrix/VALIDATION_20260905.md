# M4 continuation checkpoint — 2026-09-05

This is a bounded validation checkpoint, not a claim that the complete project,
compiler matrix, or all possible CPO compositions have been tested. Existing
worktree changes were preserved; no fast_io commit was created.

## Correctness changes retained

- Ordered concat's last-type selection folds built-in addresses instead of
  class expressions. It preserves the final object's cv-qualified type without
  imposing an ADL comma-operator requirement on otherwise valid print sources.
- Newline emission retains the complete ordinary character-put dispatcher.
  Pre-reserving even after the final writer could convert an overflow into a
  buffer hit, suppressing a valid character-put or bulk-write ADL hook. The
  existing promotion/publication markers do not authorize that substitution.
- The earlier geometric growth policy remains for non-terminal sources; the
  final one-shot source retains exact growth. Newline growth is again owned by
  its actual output CPO, including its ordinary capacity policy.

`ordered_adaptive_growth.cc` now includes deleted-comma controls for N2/N8/N9
and failures during first promotion from both a pair checkpoint and physical
overflow. `ordered_terminal_overflow_hooks.cc` separately exercises character
and range hooks, full/non-full buffers, writer failure, and hook failure.

The deleted-comma regression failed compilation before the fix. The overflow
hook regression failed at runtime before the fix. After the fixes, both
standards (C++20/C++23), the broader concat protocol test, and separate ASan and
UBSan passes succeeded. First-promotion unwinding destroys the constructed
result even when its reserve never obtains an allocation.

Both regression tests also compiled to optimized assembly for i686 Linux,
i686 MinGW, and wasm32-wasip1. These six checks establish cross-target
compilation only; they are not execution evidence on those targets.

## Measurement conditions

- Apple M4, custom Clang 23, SDK sysroot, `-march=native`, `-O3`, C++23, lld.
- Exactly one local compilation or benchmark process at a time; artifacts in
  `/tmp`. Build phases precede timed phases except for non-performance sanitizer
  checks.
- Runtime target 40 ms; separate outer 800 ms deadline. Payload sanitizer
  exercise uses 20 ms; transmit sanitizer exercise uses 16 fixed iterations
  without calibration. A timeout is retained as a failed record, never silently
  removed.
- Byte/value oracles run outside timing. Comparisons use independent validation
  digests where the fixture provides them, not calibration-dependent timed
  checksums. Transmit instead checks every preflight byte and the complete
  extent/primitive counts, then validates the iteration-dependent checksum
  against an independent closed-form expectation.
- ASan and UBSan run independently. M4 leak detection is disabled; these results
  are **not LSan evidence**.

## Completed finite matrices

| Matrix | Builds | Runtime result |
| --- | ---: | --- |
| General 0026 print/concat controls | 256 | 256 PASS; all 128 old/new digests agree |
| Existing general-control binaries, ABBA replay | none | 512 PASS |
| Existing pre/post growth-fix concat binaries, two paired rounds | none | 736 PASS |
| Final ordered concat after both ADL fixes | 92 | 368 PASS |
| Final large N8 concat, std/native, line off/on, nine lengths | 16 | 143 PASS, one old first-execution timeout |
| Replay of that large-concat timeout's complete old/new group | none | 12 PASS; original failure retained |
| 0028 scan/to/protocol/transcoder baselines | 60 | 120 PASS |
| New large-payload to/inplace_to | 84 | 1511 PASS, one new first-execution timeout |
| Replay of that to timeout's complete old/new group | none | 12 PASS; original failure retained |
| Large-payload to, eight selected cells, separate ASan/UBSan | 16 | 160 PASS; correctness only |
| Transmit, six kinds, four chunks, two outputs, old/new | 96 | 1424 PASS, 48 old FAIL, 256 UNSUPPORTED |
| New transmit, six kinds, two outputs, separate ASan/UBSan | 24 | 184 PASS, 32 UNSUPPORTED; correctness only |

The 0028 baseline run includes all four old floating-scan input shapes and old
text-to-double. The official old headers emit existing conversion warnings
under this SDK; both baselines retained those diagnostics with only
`sign-conversion` and `shorten-64-to-32` removed from `-Werror`. This is not a
missing-interface exclusion or a modification of the official baseline.

The warmed/control results still identify real work remaining. In the general
ABBA replay, geometric mean new/old was 1.00969 for print and 0.95240 for concat;
individual slow cells must not be hidden by those averages. Examples were
scatter N2/native concat approximately 2.05x, dynamic N2/std concat 1.98x, and
precise N2/obuffer print 1.43x.

The apparent approximately 50% short-concat cross-batch slowdown did not recur
in the 736-record interleaved replay. After/before geometric means were
0.99905, 0.99369, 1.00337, and 0.99632 for small/2047/2048/2049 respectively.
After the ADL fixes, complete linked disassemblies of the N8 line-off std/native
cases also remained identical to the immediately preceding retained version.

## Large-payload to: identified cause, rejected experiments

The new fixture distinguishes dynamic reserve, a 256-byte stack hint, and
scatter sources; reuse, growth, interior stop, and boundary stop; P4/P8; and
`to`/`inplace_to`. Leaf lengths surround 256 bytes, 2 KiB, and 4 KiB. The old
boundary-success controller is not treated as a semantically equivalent
baseline for the new-only boundary-stop cells.

For common cells, dynamic reuse/growth remained approximately 2.35x/2.92x old
in this run. Disassembly of dynamic growth P8 showed that old eliminated its
scratch allocation and forwarded source bytes directly to the collector. New
retained allocation plus source-to-scratch and scratch-to-collector copies.
This does not justify changing public decay value parameters into references.

Three isolated, uncommitted `/tmp` variants tested forced inlining of the
walker, growth helper, or both. All 16 builds and 144 oracle checks passed.
Explicit per-binary warmup followed by three paired rounds added 32 warmup and
432 measured records, all valid. Warmed geometric means relative to unchanged
new were 1.0466, 1.0024, and 1.0236 respectively; none removed the main gap, and
all increased text size. **No variant was applied to the repository.**

Assembly explained the limitation: inlining the growth helper caused some
capacity checks to become new out-of-line `ensure` calls; inlining both moved
the large body into an out-of-line owning lambda. Intermediate copies remained.
The large apparent first-run 255-byte penalties were also contaminated by
first-execution bias and are not reported as intrinsic optimization regressions.

Across the 36 common payload build pairs, geometric mean new/old ratios were
1.1125 for compile-and-link wall time, 1.1254 for compiler peak RSS, 1.2539 for
text bytes, and 1.0127 for complete file bytes. These are compile-and-link
measurements, not frontend-only measurements.

## Transmit: exact-fit correctness and remaining EOF overhead

The matrix covers typed/byte all, some, and until-EOF operations; chunk sizes
1/3/7/23; direct overflow and fixed-obuffer outputs; empty, one-byte, ordinary,
exact-capacity, zero-request, partial-request, and early-EOF profiles. The
fixture owns only 4096 bytes: 4097-byte profiles are explicitly unsupported,
as are exact-all requests exceeding available input. This matrix does not
establish correctness or performance beyond 4096 bytes.

New produced 736 valid measured records with no failure. Old produced 688
valid records and 48 failure records, all from 24 exact-4096/fixed-obuffer
cells. Each cell's first pilot failed preflight with exit status 3; its two
scheduled ABBA measurements were recorded as failures without running them.
These are not 48 independent crashes. Old's strict `<` capacity test in `writeimpl/basis.h`
rejects a buffer that fits exactly. With this fixture its fallback loses data:
typed-all/chunk1 consumes 4096 bytes but publishes none; typed-some/chunk1
publishes only 4095. The existing new exact-fit correction passes these cells.
The official baseline was not modified. These are preflight correctness
failures, not fast timings, and are excluded from performance comparisons.

For this runner both baselines retain the existing `unused-variable` warning
but do not promote that diagnostic to an error. Other enabled warnings remain
errors. This is separate from the two SDK conversion diagnostics allowed in
the 0028 baseline runner.

There are 344 complete old/new case-profile groups (1376 measurement records)
after exclusions. All failures, unsupported records, and the 48 valid new
records lacking an old counterpart are excluded from these ratios. In these
bounded profiles, each side's two measurements are first combined
geometrically, followed by the geometric mean of the paired new/old ratios.
The resulting ratios are 0.1677 for typed-all, 0.2102 for typed-some, 0.1685
for byte-all, 0.2096 for byte-some, 1.2293 for typed-EOF, and 1.2345 for
byte-EOF. They do not imply uniform improvements.
In particular, EOF empty fixed-obuffer/chunk1 remains approximately 60x old,
and the one-byte case approximately 15x. Allocation and call-graph causes
have not yet been established for this EOF gap. A follow-up must jointly
check these short cases and real payloads above 4 KiB without changing input
chunk semantics or weakening the oracle.

The independent sanitizer pass selects one chunk per kind (1/3/7/23/1/23),
both output protocols, and every profile above. All 24 builds and 184 legal
executions passed, with 32 unsupported records retained. Each execution uses
16 iterations and the same full-byte/extent/primitive-count oracle; no speed
statistic is drawn from instrumented binaries. Fresh Linux LSan remains
pending.

A separate reconciliation of all 3648 successful runtime and pilot records
found no raw identity, checksum, extent, or primitive-count inconsistency.
Normalization counts remain report-only, not asserted invariants.

## Print N2: diagnosis, not an applied optimization

The repeatable precise/dynamic N2 obuffer gap is an inlining boundary, not a
different precise-reserve strategy. Both baselines select ordinary dynamic
reserve and equivalent copy loops. New outlines `print_controls_buffer_impl`
into a 1028-byte body with a 64-byte frame and spills source records/view
state; old keeps the source fields and cursor in the caller's registers.
New also retains redundant null tests and cursor reloads in this tested path.
The buffer has 65 bytes and each source has at most 23: imported malloc/free
belong to a cold fallback, not executed allocations in this cell.

A narrowly scoped call-site inlining experiment is a remaining candidate,
not a retained patch. It must keep the original dispatcher, object identity,
per-leaf observations, and exception order. Current new text is already
smaller (4312 versus 4624 bytes), so runtime and text size must both be checked.
Blindly replacing the existing null-state logic with a remaining-size test
would also change the zero-bound/null-buffer writer contract.

## Artifact index

All paths below are local M4 artifacts, not committed test data:

- `/tmp/fast_io_adl_boundary_verify.BHVewl`
- `/tmp/fast_io_cpo_full_final_20260905.csv`
- `/tmp/fast_io_cpo_control_replay.06lhs717`
- `/tmp/fast_io_concat_same_session.fzeukcbb`
- `/tmp/fast_io_concat_adl_final_20260905`
- `/tmp/fast_io_concat_adl_large_final_20260905`
- `/tmp/fast_io_concat_adl_timeout_replay.uxdm0hz6`
- `/tmp/fast_io_state_baselines_full_warn_m4_20260905`
- `/tmp/fast_io_to_payload_m4_full84_20260905`
- `/tmp/fast_io_to_payload_deadline_replay.07igagg3`
- `/tmp/fast_io_to_payload_sanitizers_m4_20260905`
- `/tmp/fast_io_to_visibility_results_20260905`
- `/tmp/fast_io_to_visibility_warm.gc_i6ui9`
- `/tmp/fast_io_transmit_m4_full_20260905`
- `/tmp/fast_io_transmit_sanitizers_m4_20260905`

Fresh Linux LSan, the complete GCC 11–16 / Clang 17–23 runtime/compiler-cost
matrix, wider-character transmit coverage, and all direction-transcoder cells
remain unfinished. The separate Herb LLVM recovery/build/test/push task is not
represented as complete by this local checkpoint.
