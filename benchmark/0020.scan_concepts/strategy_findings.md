# Exploratory scan strategy evidence

This note records exploratory evidence collected while designing the formal Linux P-core experiment. The variadic-pack
section uses Apple M4/Clang 21; the owned-proxy section explicitly identifies its earlier Linux x86-64/GCC 15 audit.
Neither is the final benchmark result: no CPU-affinity claim is made for macOS, and final runtime conclusions belong to
separately pinned Linux samples taken on a freshly idle physical P-core. P-core and E-core use is opportunistic and
capped at 60% of each class; selected-core contention ends a timing batch, while cross-class activity is recorded and
judged by alternating-sample dispersion. Code-size observations identify the compiler and platform to which they apply.

## Precise no-error variadic pack growth

All cases scan `N` adjacent one-byte, exact-`void`, `noexcept` precise targets, advance the input cursor once on success,
update the same target state, and cross the same opaque observer boundary. The public variadic batch and direct-fold
control apply the target run before their aggregate cursor commit. The one-object semantic pack follows scalar precise
semantics and consumes its exact input before calling its one nothrow producer. They contain no integer, floating-point,
Unicode, or other parsing algorithm. Builds used `-O3 -march=native -std=c++23 -DNDEBUG` and instantiated one case per
executable.

| Strategy | Count | `__text` bytes | Representative M4 time |
|---|---:|---:|---:|
| Pre-optimization public variadic batch | 64 | 34,484 | 35.7--36.7 ns/op |
| Direct-fold assembly control | 64 | 1,680 | 18.2--18.5 ns/op |
| One-object semantic pack | 64 | 812 | 17.7--18.9 ns/op |
| Pre-optimization public variadic batch | 256 | 365,012 | 713--728 ns/op |
| Direct-fold assembly control | 256 | 5,656 | 71.3--76.9 ns/op |
| One-object semantic pack | 256 | 952 | 77.0--77.8 ns/op |

The measured public batch left multiple `scan_n_precise_reserve_no_error<N, ...>` functions after the inliner reached
its budget. Each recursive level had a shorter but still very large parameter list, so emitted argument moves and call
frames grew with the sum of the remaining suffix lengths rather than with `N` alone. This is direct assembly evidence
of a strategy accident, not overhead inherent to the precise scanner CPO. The working headers subsequently replaced
that recursive run application with a tuple/index-sequence fold and isolated the noncontiguous scalar fallback; those
changes require fresh opportunistically pinned Linux measurements and are intentionally not assigned unmeasured
numbers in this note.

Fresh validation has three attribution controls. The marked `pack64`/`pack256` cases permit delayed aggregate commit;
`unmarked64`/`unmarked256` use the same one-byte grammar without that marker and therefore retain the per-target cursor
publication semantics. Their timed target does not indirectly observe the input, so an optimizer may eliminate the
intermediate stores; these timings classify generated policy rather than pricing every observable publication.
`mixed-prefix64-terminal` makes a 64-target marked prefix fit in one terminal span, then enters one context-only `tail|`
scanner. Its isolated build distinguishes direct tuple/index suffix selection from the removed chain of progressively
shorter `skippings` specializations. All pack controls now have preflight, exact checksum validation, and compile-time
batch probes where strategy identity matters, but no results are recorded here until the frozen/current Linux binaries
are rebuilt from one code freeze.

A private experiment replaced the complete-run recursion with one variadic fold. It reduced the 256-target time to
about 192 ns/op but increased isolated `__text` to approximately 1.33 MiB. That negative result is important: changing
only the recursive implementation allows the compiler to inline more work but leaves the 256-parameter public ABI and
normalization graph intact. It improves one runtime number by making the compile-time/code-size problem worse.

The mixed-prefix compile-time probe exposed a second, independent policy leak. Run discovery represented the first
non-precise suffix target as `{position=0, aggregate=false}` and combined that sentinel bit with the preceding marked
target. Repeating the recursion preserved the correct 64-target extent but incorrectly classified the whole prefix as
non-aggregate, turning one cursor publication into 64. The boundary case now starts a fresh one-element precise run;
only policy bits belonging to targets inside that run are combined. The isolated mixed-prefix selector statically
proves the corrected 64-byte, 64-target, aggregate-safe prefix before any timing is accepted.

## Exploratory owned-proxy transport evidence

The following figures are an earlier Linux x86-64, GCC 15 diagnostic experiment, not the forthcoming formal pinned
benchmark. They compare the same marked precise-proxy grammar with proxy transport forced to exact references versus
the optional owned-value boundary. They motivated and bounded the admission policy; they must not be combined with the
broader frozen-header comparison or presented as a final cross-revision result.

| Diagnostic | Exact-reference control | Owned-proxy experiment | Exploratory observation |
|---|---:|---:|---:|
| Nine one-word precise proxies | 3.326 ns/op | 2.507 ns/op | -24.6% |
| Complete benchmark `.text` | 175,458 bytes | 174,978 bytes | -480 bytes |

In the same exploratory sweep, packs of 8 through 32 one-word proxies improved by approximately 21--30%. Unconstrained
48- and 64-proxy owners crossed GCC's inlining budget and generated an out-of-line public scan call with stack-argument
setup in the hot loop. This discontinuity, rather than a claim that 32 is universally optimal, is the evidence for the
Linux x86-64 cap of 32. Packs of 64 and 256 therefore remain on the exact-reference linear controller. Other ABIs use a
conservative cap of 16 until equivalent native code-shape evidence exists.

The optimization is restricted further than those size measurements alone imply. Every copied proxy must return exact
`true_type` from `scan_proxy_value_transport_safe`; be an exact unqualified non-lvalue; be complete, trivially
copy/move constructible and trivially destructible; fit within two words at ordinary word alignment; and fit the total
word-rounded ABI budget. A leading precise run must contain at least two scanners on an ibuffer source. Direct status
and general context-only paths retain references. The separately measured exception is exactly two context scanners
whose marked descriptors each occupy at most one machine word; that three-scalar-slot call shape is documented below.
Mutex dispatch first establishes one unlocked observer before applying the same source-aware classification. These
semantic and source-shape gates prevent a favorable small-pack code shape from silently redefining proxy identity or
imposing a wider value ABI where the selected controller cannot repay it.

Owned transport and aggregate cursor commit remain separate proofs. `scan_proxy_value_transport_safe` states that a
proxy copy denotes the same external scan state and has no observable object identity. In contrast,
`scan_precise_reserve_aggregate_commit_safe`, together with a `noexcept` CPO, states that intermediate stream-cursor
publications may be delayed. An implementation may select either policy without selecting the other. The focused
compile-time and runtime matrix covers marked and unmarked values, lvalue/noncopyable aliases, cv and incomplete types,
object/count/rounded-byte limits, short-buffer outlined fallback, source-shape admission, and a move-only status
observer. The Linux sanitizer randomized drivers were run only through the silent wrapper and both returned status
zero; that result validates execution but supplies no performance number.

## Pinned owner/reference and frozen-baseline results

The post-optimization binaries were built by GCC 15.2 for x86-64 Linux. Eleven owner/reference samples and nine
current/frozen samples alternated executable order on logical CPU 10 after CPU 10 and its SMT sibling 11 measured at
least 99.67% idle. Four of sixteen E-cores were compiling during the batches, within the host's opportunistic 60%
limit; current pack medians had less than 1% median absolute deviation. These figures supersede the exploratory owner
timing above for the present header state.

| Equal-work transport control | Owned | Forced reference | Change |
|---|---:|---:|---:|
| marked precise `pack9` | 2.559 ns/op | 3.446 ns/op | -25.7% |
| marked precise `pack64` | 18.945 ns/op | 19.086 ns/op | -0.7% |
| marked precise `pack256` | 137.165 ns/op | 137.622 ns/op | -0.3% |

The cap explains the shape: nine one-word proxies enter the owner specialization, whereas 64 and 256 retain the exact
reference controller. `benchmark_precise_pack<9>` shrank from 755 to 586 bytes, and complete executable `.text`
shrunk from 175,458 to 175,202 bytes. The direct status benchmark had the same address and 144-byte function size in
both binaries, confirming that the source-aware gate did not accidentally apply proxy ownership to status dispatch.
Sub-nanosecond status timing differences are therefore code-layout/noise observations, not transport evidence.

The frozen baseline exposes a separate medium/large-pack threshold problem:

| Precise pack | Current linear controller | Frozen baseline | Change |
|---|---:|---:|---:|
| 64 targets | 19.037 ns/op | 12.594 ns/op | +51.2% |
| 256 targets | 138.668 ns/op | 2,766.973 ns/op | -95.0% |

Removing recursive suffix specializations fixes the catastrophic 256-target code/inlining failure, but the same
global strategy sacrifices the old 64-target hot code shape. This is evidence for a bounded medium-pack strategy, not
for restoring recursive dispatch generally. Any refinement must retain one normalization boundary and linear template
growth, recover the 64-target assembly only below an explicit code-size threshold, and leave the 256-target path on the
current controller. Until that experiment exists, the 64-target regression is an acknowledged open cost-policy issue.

The final common matrix also measured `pack9` at 2.268 ns/op in the frozen controller and 2.687 ns/op in the current
controller. That delta is not evidence that a recursive producer is intrinsically faster: replacing the current fold
with an always-inlined recursion bounded at 16 targets produced byte-identical GCC 15 machine code. The frozen hot loop
also subtracts its two cursors unconditionally, which is undefined for the permitted newly constructed
`{nullptr, nullptr}` ibuffer representation; the current controller first excludes that empty pair. A diagnostic
integer-address extent calculation removed the extra branch, but alternating timings showed no stable improvement and
the change would have traded a proved provenance boundary for an unmeasured assumption. It was reverted. Thus the
small remaining regression is recorded as a code-schedule/correctness-boundary cost, not “fixed” with a template
threshold that changes no generated instructions.

## Required policy boundary

Small heterogeneous packs still benefit from type-directed batching: their exact extents and independent CPOs are
known at compile time, one capacity check replaces repeated checks, and a separately proved marker can reduce cursor
publication to one commit. Large homogeneous groups need a different representation.

A range/semantic-pack customization should therefore prove all of the following independently:

1. the element scanner has an exact, no-error precise contract, plus `noexcept` when aggregate commit is requested;
2. the range cardinality and aggregate extent are representable, with checked multiplication/addition;
3. the range object remains one public parameter and its producer owns iteration over the elements;
4. the complete range is proved available (or exactly staged), while the producer/commit order is stated explicitly
   rather than inferred from the word "batch";
5. the strategy has an explicit code-growth threshold so an ordinary heterogeneous variadic pack remains bounded.

The one-object control is evidence for this separation: its 256-element text section is 952 bytes and its time remains
linear, while preserving the same successful target writes and final cursor. A concept merely named "batch" is
insufficient; the representation, public parameter graph, and cursor-publication schedule are all part of the proof.

The default checked-run schedule now commits each positive extent immediately before its CPO, matching scalar precise
dispatch even though the run shares one bounds check. This remains valid for an exact-`void` CPO that may throw: at the
exception boundary, the same target has been consumed and the same applied prefix is visible as under scalar dispatch.
A type may opt into apply-all-then-one-commit only when its CPO is `noexcept` and it supplies the exact
`scan_precise_reserve_aggregate_commit_safe` `true_type` marker. `noexcept` and the marker prove different facts:
`noexcept` rules out an exceptional half-applied run; the marker asserts that missing intermediate cursor publications
cannot be observed indirectly or exposed through callbacks.

## Refill and terminal classification

### Rejected two-target scalar-inline experiment

A pinned GCC 15.2 comparison isolated a regression in the two-token `context-pack` workload. The pre-refinement current,
same-source forced-reference, and prior-current medians were respectively 15.323, 15.196, and 15.191 ns/op, while the
frozen baseline measured 9.455 ns/op. The near identity of the three current controls proves that proxy ownership and
chunked value transport are orthogonal to this result. Type classification finds no precise prefix, so the current
controller sends its only execution path through the general no-inline scalar fallback. The frozen controller instead
inlined its two scalar state machines.

GCC 15.2 assembly first made a bounded-inline experiment appear plausible. Before the experiment,
`benchmark_context_pack` was 481 bytes and called a 2,282-byte specialized scalar fallback once per iteration (2,763
bytes combined). The inline candidate emitted a 1,449-byte hot function plus a 98-byte cold clone (1,547 bytes
combined), and complete executable text decreased from 156,554 to 153,794 bytes in the matched `-O3` build. Total text
was nevertheless the wrong cost proxy. The outlined fallback had no calls on its successful path; after inlining, the
timed loop itself grew from `0x1e1` to `0x5a9` bytes and GCC lowered two exercised three-byte refill copies to
`memcpy@plt` calls. Removing one controller call therefore introduced two library calls and increased hot-loop register
pressure even though cold and duplicated text became smaller.

The candidate had a sound semantic and template-growth boundary: only two targets with a leading precise run of length
at most one were admitted; adjacent aggregate-capable precise targets and all packs of three or more remained outlined.
It received the existing named normalized lvalues and used the same built-in `&&` fold, so it performed no second decay
or proxy copy and preserved scanner order, EOF short circuit, cursor publication, exceptions, context state, and mutex
behavior. Those proofs did not establish performance. Eleven alternating samples of binaries built with the identical
`g++-15 -O3 -march=native -std=c++20 -DNDEBUG` command on an idle CPU 10/11 pair measured 16.749 ns/op for inline versus
14.004 ns/op for the exact outlined control: a 19.6% regression, with 0.19% median absolute deviation for inline. The
policy and its focused admission test were therefore removed. The retained strategy is the existing outlined linear
fold; smaller total text is recorded here only as evidence that text size cannot substitute for hot-path inspection.

### Retained two-target value-ABI refinement

The rejected result above concerns inlining the state machines, not every possible context-pair policy. A later
experiment retained the exact outlined controller and changed only how two explicitly transport-safe normalized
descriptors cross its ABI. The reference specialization passed two proxy addresses; the candidate passed the two
one-word pointer descriptors themselves. `scan_proxy_value_transport_safe` proves that repeated copies denote the same
external targets and have no descriptor identity, while the existing triviality and ABI filters independently reject
cv-qualified expressions, lvalue aliases, nontrivial objects, and indirect argument classes. The context admission adds
an exact-two and one-machine-word-per-object bound. Including the separately borrowed input observer, the call carries
three scalar slots, so the rule neither splits an AAPCS32/MIPS o32 two-word aggregate nor increases stack-ABI payload
relative to the reference form.

With GCC 15.2 on an initially 100% idle physical-core 7 pair (timed CPU 14, sibling 15), nine alternating samples of
20 million operations measured medians of 19.161 ns/op for the same-source forced-reference executable and 18.422 ns/op
for the one-word value policy, a 3.9% reduction. The caller/fallback pair shrank from 505 + 2,350 bytes to 451 + 2,312
bytes. A rebuild from the final argument/result-ABI split reproduced the direction at 20.187 versus 19.504 ns/op
(-3.4%) on another idle physical P-core. An Apple Clang 21 AArch64/M4 single-process control measured medians of
21.847 and 21.766 ns/op respectively; that 0.4% difference is treated as neutral rather than claimed as an
architecture-wide speedup.

A homogeneous runtime-loop alternative is a useful negative control. It reduced the outlined fallback from 2,312 to
1,280 bytes, but its median was about 9.5% slower than the value-expanded pair because the additional target loop and
shared state-machine schedule outweighed the instruction-cache saving. That alternative was removed. The retained
policy therefore owns only the two one-word descriptors; it does not inline, type-erase, loop over, or re-normalize the
context scanners. Single scanners remain on direct scalar dispatch, packs larger than two retain exact references, and
direct status CPOs remain reference-only.

Current context-only and explicitly terminal-equivalent hybrid scanners compile to the same isolated 1,324-byte text section on a refillable source
and measured in the same approximately 35--39 ns/op band. The terminal hybrid specialization is a separate 308-byte
text section and measured about 1.80--1.93 ns/op. This supports the source-dependent classification: admitting both
contiguous and context capabilities does not penalize the refill state machine, while a terminal source avoids it.

The current exact-staging case for a four-byte precise target split across `3 + 1` bytes measured approximately
6.3--7.3 ns/op and produced the expected checksum. The frozen baseline produced a different checksum and therefore
did not execute equivalent semantics; its lower timing is invalid as a performance comparison. Likewise, the terminal
scalar baseline hoisted invariant target bytes outside the loop, whereas the current short-read/refill correctness
branches prevented that transformation. Those two code-shape observations are diagnostic only and must not be
published as direct speed regressions.
