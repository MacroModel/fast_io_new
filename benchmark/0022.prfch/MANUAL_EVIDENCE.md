# Processor-manual evidence and policy boundary

This note records how processor-vendor documentation is used to design fast_io's prefetch experiments.  A manual can
establish architectural semantics, microarchitectural costs, and access patterns worth testing.  It cannot establish
that a hint is profitable in a particular print, concat, or scan specialization: compiler scheduling, the caller's
memory provenance, cache state, and the amount of useful work before the demand access remain properties of the final
program.  Production activation therefore follows this evidence order:

1. prove that the compiler emits a valid instruction for the selected ISA, tune, address form, and language mode;
2. use the processor manual to reject implausible levels, distances, and access shapes;
3. inspect the complete public fast_io entry point, including ABI decay, lookahead, branches, and copy lowering; and
4. enable a tune family only after repeated, core-bound measurements on that family retain the hot negative controls.

Steps 1--3 may add an experimental concept.  Only step 4 may add a production site to an allow-list.  This distinction
keeps an ISA-wide capability such as AArch64 `PRFM` from becoming an unsupported claim about every AArch64 core.

## Intel data prefetch

The [Intel 64 and IA-32 Optimization Reference Manual](https://www.intel.com/content/www/us/en/developer/articles/technical/intel64-and-ia32-architectures-optimization.html)
is the primary microarchitecture reference.  Intel's worked
[HBM software-prefetch study](https://www.intel.com/content/www/us/en/developer/articles/technical/qcd-performance-optimization-with-hbm.html)
makes the policy boundary unusually explicit: hardware prefetchers recognize streaming and strided access but do not
cross a 4-KiB page boundary; software-prefetch distance which is too short cannot hide latency, while a distance which
is too long can evict the line before use.  Extra requests can also burden memory-pipeline queues.

That evidence determined the benchmark shape, not the production result.  The current source descriptors are shuffled
and page-separated, so hardware cannot infer the address of the next descriptor from the sequential copy inside the
current one.  fast_io hints only the first line of the next proved-live, nonempty source and leaves the current
4/16-KiB run to `memcpy` and the hardware prefetcher.  The retained P-core and E-core measurements in `RESULTS.md`, not
the manual alone, authorize `x86_intel_hybrid`.  They do not authorize the broader Intel Core bucket.

Instruction prefetch has a separate proof.  Intel's architectural documentation requires the effective
`PREFETCHIT0/PREFETCHIT1` form to use 64-bit RIP-relative addressing.  Consequently a compiler accepting a pointer
operand is not sufficient evidence: fast_io separates dynamic, direct-symbol, and explicitly local-symbol concepts,
and the GCC/Clang assembly checks in `RESULTS.md` verify their different lowering.

## AMD Zen candidates

AMD's [AMD64 Architecture Programmer's Manual, Volume 1](https://www.amd.com/content/dam/amd/en/documents/processor-tech-docs/programmer-references/24592.pdf)
recommends software prefetch where memory latency can be hidden, one hint per cache line, useful consumption of the
prefetched data, and normally unit-stride access.  AMD publishes separate
[Zen 4 and Zen 5 optimization guides](https://docs.amd.com/r/en-US/57368-uProf-user-guide/Useful-URLs?contentId=egjf_QgPZoI0hKkpK387fw),
which is evidence that a single `x86_amd_zen` name is a source-level family rather than a uniform performance model.

The fast_io scatter chain has a mixed shape: descriptor-to-descriptor addresses are discontinuous, but each selected
payload is then consumed sequentially.  One hint for the next descriptor is intentionally more conservative than
prefetching every cache line; whether that single hint arrives early enough on Zen remains a hardware question.
AMD's current uProf documentation exposes an `INEFFECTIVE_SW_PF` event for software prefetches which hit in-core data
or an already allocated miss request.  A retained Zen run should collect that event when the exact model supports it,
using the model's official event mapping rather than a repository-wide raw event number.

No Zen machine has been measured in this worktree.  The generic primitive and experimental platform concepts are
therefore available, but the concat and print production site predicates remain false for AMD Zen.

## Arm and Neoverse candidates

The [Neoverse N2 Software Optimization Guide](https://developer.arm.com/documentation/109914/latest/) documents
`PRFM` as consuming the load machinery and gives its implementation throughput.  Its memory-routine guidance focuses
on aligned, unrolled load/store pairs for contiguous copies.  This supports keeping software hints away from the
current contiguous `memcpy`; it does not prove that hinting a discontinuous next descriptor is profitable.

Arm's [Neoverse N1 prefetch exercise](https://learn.arm.com/learning-paths/servers-and-cloud-computing/top-down-n1/optimize-1/)
requires experimenting with a prefetch distance and notes that the result changes with system memory bandwidth.  That
is exactly why fast_io represents an AArch64 capability separately from a tune/site policy.  The architecture can
lower `PLD`, `PST`, and `PLI` correctly while Apple, Neoverse N1/N2/V-series, and other implementations keep distinct
activation evidence.

The Apple M4 negative control in `RESULTS.md` verified lowering but did not produce stable enough runtime evidence for
an AArch64 default.  Neoverse remains disabled until the portable snapshot is run on an actual pinned Graviton or
other Neoverse host.

## Required cross-CPU snapshot

Use `run_cpu_snapshot_linux.sh` on every new Intel, AMD, or Arm host.  Its fixed public operation shape is 32
discontinuous sources with 4- and 16-KiB payloads under both hot and cache-pressure conditions.  Preserve all snapshot
metadata and run at least three independent processes for a policy decision.  A cold improvement is insufficient if
the corresponding hot control regresses; one favorable process is insufficient to admit a tune family.

Where counters are available, add only model-documented events: ineffective software prefetch on AMD, load/cache
refill and queue-pressure evidence on Intel, and the matching Neoverse PMU events on Arm.  Counters explain a timing
result but never replace the paired baseline, because a lower miss count can still lose to hint, branch, or code-size
overhead.
