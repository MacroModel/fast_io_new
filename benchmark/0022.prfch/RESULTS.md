# Retained prefetch evidence

This file records the measurements which currently constrain the production policy.  It is not a claim that a
software hint is universally profitable: prefetch timing depends on the core, cache state, source layout, compiler,
and the work available between the hint and the demand access.  Raw CSV files remain in `/tmp` on the measured host;
the commands below make each retained aggregate reproducible without embedding machine-specific artifacts in the
repository.

## x86 instruction-prefetch compiler split

The architectural restriction is stronger than the C++ pointer-shaped builtin interface.  Intel documents
PREFETCHIT0/PREFETCHIT1 as effective in 64-bit mode with RIP-relative addressing and as NOPs otherwise.  A Binutils
2.45 probe confirmed that distinction: `prefetchit1 target(%rip)` remained PREFETCHIT1, whereas
`prefetchit0 (%rax)` produced the expected assembler warning and disassembled as `nopl (%rax)`.  The corresponding
compiler and assembler provenance is recorded in the [Intel SDM](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html),
[Binutils PREFETCHI patch](https://sourceware.org/pipermail/binutils/2022-October/123911.html), and
[GCC PREFETCHI tests](https://gcc.gnu.org/pipermail/gcc-patches/2024-July/658336.html).

fast_io therefore models three independent capabilities instead of treating
`__builtin_ia32_prefetchi` as one portable operation:

- the generic/dynamic `prfch<prfch_mode::instruction>(address)` API preserves Clang's pointer-shaped x86 builtin but
  is a no-op on genuine GCC x86; the Clang lowering can be an effective RIP-relative hint or an architecturally
  ignored register-address form;
- `prfch_instruction_direct<&function>()` makes a statically named target available to Clang's direct lowering, so an
  internal or hidden target can become RIP-relative while a preemptible target may still use a GOT load and register
  operand; genuine GCC deliberately keeps this convenience a no-op because the NTTP alone is not a binding proof; and
- `prfch_instruction_local<prfch_local_function_t<&function>>()` is enabled for both Clang and genuine GCC under
  `-mprefetchi`; its token is an explicit caller assertion that the final symbol is non-preemptible and locally bound.

GCC 15.2 with `-O2 -fPIC -mprefetchi` defined `__PREFETCHI__`.  Internal-linkage and hidden-visibility targets passed
through the local token emitted `prefetchit0 symbol(%rip)` at both `-O2` and `-O0`; the generic and direct APIs emitted
no hint.  A deliberately false local assertion for a default-visible target produced GCC's RIP-relative diagnostic,
failed under `-Werror`, and emitted no hint without `-Werror`.  Clang's official
[`prfchiintrin.h`](https://clang.llvm.org/doxygen/prfchiintrin_8h_source.html) exposes a pointer-shaped builtin, and
Clang 23 accepted that false assertion but lowered the preemptible target through a GOT load to
`prefetchit0 (%register)`.  Thus Clang trusts the token while GCC can diagnose common false assertions; neither
behavior weakens the token's caller-side binding precondition.

The direct API also remains available on Arm targets, where PLI accepts a general address and does not inherit x86's
RIP-relative premise.

## Linux P-core screen

The Linux host was an Intel Core i9-14900HX.  CPU 10 is the first logical processor of physical P-core 5; its sibling
was left unused.  GCC 15.2 with `-O3 -march=native` selected `__tune_alderlake__`.  Every process was bound with
`taskset`, and `mpstat` reported the selected CPU at least 99% idle immediately before the run.

The initial 64-case screen used 32/128 descriptors, 64/256/512/4096-byte payloads, contiguous/discontinuous layouts,
hot/LLC-pressure modes, and both copy directions.  Seven alternating paired samples were retained per case.  The
important negative result was the hot small-payload region: at 256 and 512 bytes, lookahead plus one L1/keep hint was
about 17--31% slower in several 32-descriptor cases.  A platform capability can therefore never enable this loop by
itself, and 256 bytes is not a defensible production threshold.

The follow-up read-only experiment restricted the discontinuous layout to 4096/16384-byte payloads and
32/128/1024 descriptors.  One 17-sample run and two independent 21-sample runs used three deterministic layout seeds,
256 MiB of hot work per sample, and a 128 MiB cache scrub before every cold operation.  Across the three process-level
medians, the geometric prefetch/baseline ratios were:

| Cache | Descriptors | Payload | Geometric ratio | Winning runs |
|---|---:|---:|---:|---:|
| hot | 32 | 4096 | 1.0013 | 1 / 3 |
| hot | 32 | 16384 | 0.9987 | 2 / 3 |
| hot | 128 | 4096 | 0.9996 | 2 / 3 |
| hot | 128 | 16384 | 1.0011 | 1 / 3 |
| hot | 1024 | 4096 | 0.9994 | 2 / 3 |
| hot | 1024 | 16384 | 0.9998 | 2 / 3 |
| cold | 32 | 4096 | 0.9899 | 2 / 3 |
| cold | 32 | 16384 | 0.9823 | 3 / 3 |
| cold | 128 | 4096 | 0.9868 | 3 / 3 |
| cold | 128 | 16384 | 0.9878 | 3 / 3 |
| cold | 1024 | 4096 | 0.9759 | 3 / 3 |
| cold | 1024 | 16384 | 0.9898 | 3 / 3 |

This supports only a narrow read policy: a long descriptor chain, both the current and next nonempty ranges at least
4 KiB, a source with explicit ordinary-cacheable provenance, and a separately admitted x86 tune family.  The mirrored
write results were not stable, so they do not authorize a write policy.

## Print materializer boundary confirmation

A separate narrow check validated print's identical irregular-source-to-contiguous-destination operation after its
large-payload copy was placed behind a deliberate `noinline` boundary.  That boundary prevents GCC from cloning the
dynamic memcpy and later prefetch sites throughout a 32-argument specialization, but it also adds one ordinary call
per source.  The comparison therefore measured the complete enabled materializer, including that call boundary,
against the historical disabled materializer rather than timing an isolated prefetch instruction.

The run used the same Linux host and GCC 15.2 `-O3 -march=native`, was bound to CPU 10 (physical P-core 5), and began
with `mpstat` reporting that CPU 100% idle.  Thirty-two discontinuous sources were measured with 17 alternating paired
trials; the table reports the median enabled/disabled ratio:

| Cache | Payload per source | Enabled / disabled |
|---|---:|---:|
| hot | 4096 | 0.9969 |
| cold | 4096 | 0.9673 |
| hot | 16384 | 0.9972 |
| cold | 16384 | 0.9973 |

This is a boundary-cost confirmation, not an expansion of the policy envelope.  It shows that the code-size boundary
did not consume the retained benefit: the 4-KiB cold case improved by about 3.3%, while both hot controls and the
16-KiB cold case remained within roughly 0.3% on the favorable side of parity.

## Hybrid E-core confirmation and final tune boundary

The compile-time `x86_intel_hybrid` category does not determine whether the operating system will schedule a thread on
a P-core or an E-core.  A second confirmation therefore ran the same 32-source 4/16-KiB hot/cold comparisons on CPU 26,
an E-core of the same i9-14900HX.  The selected CPU sampled 98--100% idle before each process.  Print used three
independent processes with 17 alternating trials each; concat used three seeds with 21 paired samples each.  The table
reports the geometric mean enabled/baseline ratio and the complete three-process range:

| Site | Cache | Payload | Geometric ratio | Three-process range |
|---|---|---:|---:|---:|
| print | hot | 4096 | 0.98248 | 0.97956--0.98600 |
| print | cold | 4096 | 0.98999 | 0.98777--0.99240 |
| print | hot | 16384 | 0.99505 | 0.99383--0.99588 |
| print | cold | 16384 | 0.99483 | 0.98736--0.99891 |
| concat | hot | 4096 | 0.98739 | 0.98707--0.98798 |
| concat | cold | 4096 | 0.97319 | 0.96586--0.98572 |
| concat | hot | 16384 | 0.99546 | 0.99445--0.99658 |
| concat | cold | 16384 | 0.98823 | 0.98546--0.99004 |

No hybrid E-core control regressed, so the production site remains enabled for `x86_intel_hybrid`.  This result does
not cover the much broader `x86_intel_core` bucket (Core 2 through current server/client cores) or AMD Zen.  Both are
therefore excluded from the concat and print site predicates until independent hardware evidence exists; the broad
experimental platform concepts remain available without authorizing either hot loop.

## Apple M4 negative control

Apple Clang 21.0 on an Apple M4 emitted the expected AArch64 `PRFM` instructions.  The runtime harness was single
threaded, as required by the local-resource budget, but macOS offered no supported equivalent of the Linux physical
core pin.  Three independent seeds tested 128/1024 descriptors and 4096/16384-byte discontinuous payloads with
17--25 paired samples.

The result did not support a default AArch64 site policy.  For example, the three-run cold geometric ratio was 0.9765
for 128 x 16384 bytes but 1.0331 for 128 x 4096 bytes.  The 1024-descriptor hot/cold process medians varied by as much
as roughly eight percent in either direction.  Correct instruction lowering is therefore retained, while automatic
AArch64 activation remains disabled pending a pinned or otherwise repeatable core-specific result.

## Reproduction boundary

Use `run_linux.sh` exactly as documented in `README.md`, selecting one explicitly permitted, idle logical CPU and
leaving its SMT sibling unused when the topology exposes one.  The CPU 10 command below reproduces the retained host's
historical P-core run; it is not a general requirement that another validation machine use a P-core.
The retained Linux follow-up used:

```sh
CPU=10 COUNTS='32 128 1024' PAYLOADS='4096 16384' \
LAYOUTS=discontinuous CACHE_MODES='hot cold' DIRECTIONS=read \
SAMPLES=21 WARMUPS=3 TARGET_BYTES_PER_SAMPLE=268435456 \
COLD_BYTES=134217728 benchmark/0022.prfch/run_linux.sh
```

Cold mode is cache-pressure evidence, not an architectural proof that every relevant line was evicted.  Policy
changes must retain the hot negative controls and use multiple process-level seeds; a single favorable median is not
sufficient.
