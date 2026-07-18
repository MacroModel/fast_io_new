# Scanner concept and dispatch benchmark

This directory isolates scanner *protocol composition*. It does not compare integer, floating-point, Unicode,
locale, regular-expression, or other parsing algorithms. Every benchmark token is a literal delimiter, a fixed byte
record, or a short delimited text field. Consequently, a result from this directory is evidence about normalization,
capability recognition, state dispatch, buffering, packing, or locking only; it is not evidence that one library has a
faster `atoi`, `ftoa`, or general-purpose parser.

`scan_dispatch.cc` currently measures fast_io dispatch variants and a common noinline observer baseline. It is not, by
itself, a fast_io-versus-scnlib benchmark. A cross-library number must not be published until both sides execute the
same grammar, consume the same range, expose the same error semantics, and account separately for work that exists on
only one public API path.

The large precise-pack cases include two assembly controls. `direct-fold control` invokes the same no-error precise
CPOs and applies the complete run before one aggregate cursor commit. The fixed-record fixture explicitly supplies
`scan_precise_reserve_aggregate_commit_safe`, so this is the marked library schedule as well as a code-shape control;
it expresses target traversal as one fold instead of the historical recursive variadic controller. It deliberately
omits the public path's alias/source normalization and aggregate bounds check, so it is a lower-bound code-shape
control rather than a decomposition of forwarding cost. `semantic pack`
presents the same homogeneous target array as one scanner object whose precise CPO owns the loop. It therefore follows
the ordinary single-precise-scanner rule: exact input is consumed before the one CPO call. Both orders have the same
successful observable result for this fixture, but only the explicit marker proves that delaying each intermediate
cursor publication is generally unobservable. Neither control is a competing public API measurement: together they
distinguish parameter-forwarding overhead from the lower code shape available to a range/semantic-pack protocol.

`unmarked64` and `unmarked256` use a second proxy with the same one-byte precise CPO, target writes, source, and
checksum, but deliberately omit the aggregate-commit marker. They classify and verify the default one-bounds-check
plus commit-before-each-CPO schedule against the marked fixed-record pack. Their timing is the optimizer's result for a
target that does not indirectly observe the source: dead-store elimination may remove intermediate cursor stores, so
these cases are not a universal price for observable cursor publication. `mixed-prefix64-terminal` places 64 marked
one-byte precise targets before one context-only `tail|` target in a terminal buffer. The complete precise prefix fits
the current span; consequently this case isolates the transition to the heterogeneous suffix rather than a refill or
short-buffer fallback. Its preflight proves 64 prefix writes, a consumed delimiter, context-not-contiguous dispatch,
one context commit, and a final cursor at the terminal end.

The frozen-header build defines `FAST_IO_SCAN_CONCEPT_BASELINE`. A tuple is paired with that build only when its
untimed preflight proves the same observable protocol. The baseline `has_ibuffer_underflow_never_define` detector
mistakenly passes its input observer to `output_stream_ref`, so a valid exact-`bool` terminal marker is not recognized;
reporting its resulting context path as “terminal hybrid/contiguous” would be false. Exact refill, context/hybrid,
mixed-prefix, and unmarked-commit cases are also current-only: the frozen implementation either produces different
target/cursor state or follows the aggregate publication schedule that those cases were added to distinguish. The
noncopyable reference-alias tuple is current-only because the baseline eagerly materializes the alias by value. The
`scan-common` runner contains only cases whose preflight and checksum succeed for both revisions; `scan-current-only`
records the corrected/admitted strategies without assigning a misleading speedup or regression to unequal semantics.

## fast_io protocol map

The public scan pipeline first normalizes each target and then selects a source strategy:

1. `io_scan_alias` tests `alias_scannable<T>` with the target's exact value category. A custom alias may be a proxy
   value or a stable proxy reference. An ordinary target becomes `parameter<T&>`; an existing manipulator remains an
   lvalue.
2. `io_scan_forward<Char>` tests `status_io_scan_forwardable<Char, T>` with that normalized value category. A stable
   lvalue result remains a reference and a prvalue result is owned for the enclosing call. Neither forwarding concept
   claims that its result is a scanner: the downstream scanner concepts make that separate proof.
3. The input observer is selected. A complete mutex protocol is entered before every other strategy, so a high-level
   status scanner cannot bypass the stream lock.
4. An exact-`bool` `status_scan_define(input, targets...)` is the highest-level scan shortcut. Otherwise an ibuffer
   dispatches each normalized target by the capability table below.
5. For an ibuffer controller only, a small pack of normalized proxy values may cross one additional by-value boundary.
   This is an optional ABI/code-generation policy, not another scanner capability. Admission requires the proxy
   author's exact-`true_type` transport marker, conservative object and pack bounds, and a leading run of at least two
   precise scanners. Every non-admitted pack remains on the general exact-reference controller.

The following table lists all active scalar scanner combinations. “Terminal” means that the input observer advertises
`ibuffer_underflow_never`; a refillable stream cannot assume that the current chunk contains a complete token.

| Precise | Context | Contiguous | Terminal query returns `true` | Query absent/returns `false` | Selected behavior |
|---:|---:|---:|---|---|---|
| yes | any | any | precise | precise | The exact type-level extent has precedence. Use the current chunk when possible, otherwise exact staging across refills. |
| no | yes | yes | contiguous only with an exact terminal-equivalence marker; otherwise context | context | Hybrid scanner: the one-shot CPO is selected only when the type explicitly proves terminal contiguous/context equivalence and the run-time terminal query succeeds. |
| no | yes | no | context | context | Incremental scanner with one state object and explicit EOF transition. |
| no | no | yes | contiguous | ill-formed | A one-shot scanner is sound only when the current range is known to be complete. |
| no | no | no | ill-formed | ill-formed | Forwarding/alias recognition alone does not make a target scannable. |

These rows compose orthogonally with the following source and pack properties:

- `has_ibuffer_underflow_never_define` proves only that an exact-`bool` terminal query exists. Hybrid dispatch calls
  `ibuffer_underflow_never(input)` for the current observer; presence of the CPO alone never authorizes the contiguous
  path. One observer type may therefore select contiguous behavior for a terminal instance and context behavior for a
  refillable instance at run time.
- A complete input-mutex protocol wraps the complete target pack once, then recursively scans an unlocked observer of
  the same character type.
- An exact-`bool` `status_scan_define` consumes the complete normalized pack and precedes ibuffer-level strategies.
- Optional proxy ownership is source- and controller-shape-aware. Direct status dispatch retains references because it
  has no outlined scalar controller from which proxy addresses must be recovered. General context-only packs retain
  references as well. There are two measured ownership shapes: an ibuffer pack whose leading precise classification
  has length greater than one, and exactly two context scanners whose explicitly transport-safe descriptors each fit
  one machine word. The latter keeps the state machines outlined while replacing two proxy addresses with two equal-size
  values; its one-input-plus-two-value call shape remains within the small-register envelope of AAPCS32 and MIPS o32.
  When a mutex is present, the public entry first locks and materializes one unlocked observer, then classifies that
  source while forwarding the already-normalized exact target categories; aliasing and character forwarding are not
  repeated.
- Adjacent exact-`void` precise scanners may share one aggregate availability check. The default schedule publishes the
  scalar cursor after each positive extent and before invoking that target's CPO, so even a throwing customization
  retains the scalar exception boundary. A run uses apply-all-then-one-commit only when every target is both `noexcept`
  and returns exact `true_type` from `scan_precise_reserve_aggregate_commit_safe`. `noexcept` prevents exceptional
  half-application, while the independent marker proves that a CPO cannot observe or expose the missing intermediate
  cursor publications. Fallible `parse_code` scanners remain scalar. Aggregate extent overflow disables batching; it
  is not a protocol failure.
- A large homogeneous variadic run is a separate resource problem from extent overflow. Even when every element is
  one byte, recursively forwarding dozens or hundreds of independently typed function parameters can produce
  superlinear machine-code growth. The `pack64`, `pack256`, `fold64`, `fold256`, `semantic64`, and `semantic256`
  controls make that cost visible; a future range/semantic-pack admission policy must keep its target group behind one
  object rather than merely replacing the recursive body with a larger variadic fold.
- A successful precise prefix followed by a heterogeneous scanner is also a code-growth boundary. Dispatch should
  enter the suffix once through tuple/index selection; recursively discarding one prefix argument per specialization
  recreates a chain of progressively shorter ABI signatures even when the hot prefix application itself is linear.
  `mixed-prefix64-terminal` is the isolated proof case for this transition.
- Outlining is itself a measured cost policy. `context-pack` provides the negative control: an exact-two inline scalar
  experiment reduced complete executable text but enlarged the timed hot loop and introduced two `memcpy@plt` calls.
  A matched pinned GCC 15.2 run was 19.6% slower, so the context pair retains one outlined linear fold. A separate
  one-word value-ABI refinement leaves that outline intact and improved a later 20-million-iteration pinned median by
  about 3.9%; replacing the pair with a smaller runtime loop was 9.5% slower and was rejected. Semantic equivalence,
  smaller text, and a bounded template graph are necessary evidence, but none substitutes for timing the hot schedule.
- `iterative_scannable` and `iterative_contiguous_scannable` are recognition-only vocabulary for a disabled
  repeated-extraction experiment. No ordinary `scan`, `parse_by_scan`, or `to` path selects them.

### Owned normalized-proxy transport

`scan_proxy_value_transport_safe(io_reserve_type_t<Char, Proxy>)` is an explicit semantic opt-in and must return exactly
`std::true_type`. Trivial copyability alone is insufficient: a trivially copyable descriptor can still use its own
address as identity, contain a self-relative pointer, or expect mutations of the original descriptor object to remain
visible. The marker promises that a bitwise-equivalent proxy denotes the same externally owned scan state and that no
scanner CPO observes the proxy object's address. The library supplies this proof only for `parameter<T&>`: copying that
wrapper duplicates a language reference to the same target. It deliberately does not mark owning `parameter<T>`.

The entry policy combines that semantic proof with all of the following independent ABI guards:

- the normalized expression must be an exact unqualified non-lvalue; lvalues and `const`/`volatile` xvalues remain
  references so by-value deduction cannot erase cv-qualification, change CPO overload selection, or lose identity;
- the proxy must be complete, trivially copy-constructible, trivially move-constructible, and trivially destructible;
- each proxy is inside the target-specific direct-argument envelope, the controller's at-most-two-word ceiling, and
  ordinary `size_t` alignment; asymmetric structure-result rules are irrelevant because these proxies are arguments;
- the pack has at least two elements and no more than 32 elements on Linux x86-64, or 16 on an ABI without equivalent
  native evidence; and
- the sum of the *individually word-rounded* proxy sizes is at most `max_count * sizeof(size_t)`. Per-object rounding is
  required because, for example, a nine-byte object consumes two ordinary eight-byte ABI slots even though summing its
  raw language size would understate the call footprint.

The checks are deliberately nested. Incomplete, cv-qualified, oversized, or over-aligned types are rejected before a
later `sizeof`, trait, or pack fold can be instantiated. Once every element is bounded by two words and the count is
bounded, the final rounded-size sum is mathematically unable to overflow `size_t`. The admitted helper owns compact
proxy values once; all scanner CPOs still receive named lvalues. Its policy bit is propagated through suffix dispatch,
so one controller cannot change transport ABI halfway through a pack. A short-buffer failure enters an outlined
by-value scalar fallback, whereas the general path is a separately instantiated exact-reference fallback that accepts
noncopyable aliases and unbounded packs.

The context-pair refinement is stricter than this general owner. It requires exactly two context-capable proxies and
caps each at one machine word. It therefore removes an address indirection without increasing the three-slot fallback
payload on ABIs with a smaller integer-register window. The direct one-scanner path, direct status CPOs, context packs
of any other cardinality, and context descriptors wider than one word remain on exact references.

This transport proof is orthogonal to `scan_precise_reserve_aggregate_commit_safe`. The transport marker proves that
replacing a proxy object with an equivalent copy is unobservable. The aggregate-commit marker proves that replacing
per-target cursor publications with one final publication is unobservable, and dispatch additionally requires the
relevant precise CPO to be `noexcept`. Neither statement implies the other; an owned pack still follows the ordinary
per-target publication schedule unless every element independently satisfies the aggregate-commit contract.

## Isolated code-shape builds

The following definitions instantiate one strategy only. They are intended for `objdump`, `llvm-mca`, compiler time,
and text-section measurements; excluding unrelated templates is essential when attributing code size:

- `FAST_IO_SCAN_CONCEPT_BENCH_PRECISE_ONLY`
- `FAST_IO_SCAN_CONCEPT_BENCH_PRECISE_REFILL_ONLY`
- `FAST_IO_SCAN_CONCEPT_BENCH_TERMINAL_ONLY`
- `FAST_IO_SCAN_CONCEPT_BENCH_CONTEXT_ONLY`
- `FAST_IO_SCAN_CONCEPT_BENCH_HYBRID_ONLY`
- `FAST_IO_SCAN_CONCEPT_BENCH_PACK_COUNT=N`
- `FAST_IO_SCAN_CONCEPT_BENCH_UNMARKED_PACK_COUNT=N`
- `FAST_IO_SCAN_CONCEPT_BENCH_DIRECT_PACK_COUNT=N`
- `FAST_IO_SCAN_CONCEPT_BENCH_SEMANTIC_PACK_COUNT=N`
- `FAST_IO_SCAN_CONCEPT_BENCH_MIXED_PREFIX64_ONLY`

The benchmark fixture can additionally define `FAST_IO_SCAN_CONCEPT_FORCE_REFERENCE_PROXY_TRANSPORT` to withhold its
transport markers. This produces an exact-reference control with the same grammar, target writes, source, and scanner
CPOs; it is not a public library configuration switch.

Only one definition may be active, and every `*_PACK_COUNT` value must be positive.
`FAST_IO_SCAN_CONCEPT_BENCH_TERMINAL_ONLY` also runs the dispatch preflight before
the timer: the current observer's terminal query must return `true`, exactly one contiguous CPO must run, and no context
CPO may run. The refill cases deliberately split a four-byte precise record at `3 + 1` and a delimiter token into
three-byte chunks, so an assembly report cannot accidentally label a terminal fast path as a refill-capable
implementation.

Every isolated pack mode and the mixed-prefix mode runs an untimed one-operation preflight, times the requested
workload, then compares its returned checksum after the timer stops. The marked/unmarked preflights also distinguish
their one-versus-per-target cursor publication schedules; compile-time probes prove the 64-target marked prefix is one
aggregate-safe run before its context suffix. This keeps validation out of the measurement while making a silent
classification, return-code, or target-write regression fail instead of producing a plausible timing.

## Exact contracts audited by the matrix

`tests/0002.printscan/scan_protocol_composition_matrix.cc` separates compile-time admission from runtime behavior. Its
fixtures use only literal characters, separators, and fixed records.

| Family | Compile-time proof | Runtime proof |
|---|---|---|
| Alias | Exact lvalue/rvalue overload category; proxy value versus noncopyable proxy reference | Public scan retains the referenced proxy and does not copy it |
| Character forwarding | Exact category of `status_io_scan_forward`; value and stable-reference results | Public alias -> forward -> scanner path updates the original target |
| `parameter<T&>` | Ordinary targets normalize to the wrapper; scanner CPO lookup uses the wrapper tag | Precise, contiguous, and context scanners defined on the wrapper all run through public APIs |
| Proxy value transport | Exact opt-in result; exact non-lvalue category; completeness, trivial special members, size, alignment, count, rounded ABI-byte boundaries, and precise-prefix/context-pair shape | Marked nine-target hot path, two-target context path, and short-buffer outlined fallback; cap-plus-one, unmarked trivial, mixed marked/unmarked, cv-qualified, over-aligned, oversized, and noncopyable-reference fallbacks; move-only status observer remains accepted |
| Precise reserve | Exact `size_t`, constant expression, pointer-difference bound, and exact `void`/`parse_code` define result | Zero extent, fixed record, and fallible one-byte delimiter |
| Contiguous | Exact `parse_result<const Char*>` result | Valid delimiter cursor and rejection of an iterator outside the supplied range |
| Context | Valid mutable default-constructible state object, exact context result, exact EOF code | Cross-refill text, no-progress rejection, escaped-iterator rejection, EOF-partial rejection, and optional rewind |
| Status source | Exact real target-pack expression and exact `bool` result | Whole-pack status dispatch, with the mutex variant covered by `scan_concept_matrix.cc` |
| Iterative vocabulary | Exact iterative return categories | Deliberately no ordinary runtime dispatch |

Runtime range validation is intentionally separate from concept recognition. C++ concepts can prove the *type* of a
returned iterator but cannot prove that a particular runtime pointer lies in `[first, last]`. Both contiguous and
context dispatch therefore validate the closed range before committing a stream cursor. Likewise, a context scanner
returning `partial` without consuming available input is structurally well-typed but dynamically invalid because it
would otherwise make no progress.

The alias and status-forward CPOs intentionally impose no common result class: a project may return a proxy value or a
stable reference. This flexibility carries a lifetime obligation. A customization must not return a reference to an
adapter object that expires when the forwarding expression ends. The matrix proves the supported stable-reference and
owned-value categories; it does not treat a dangling customization as a benchmark case.

`tests/0002.printscan/scan_proxy_value_transport_policy.cc` is the focused boundary proof. It statically exercises the
platform count cap and cap-plus-one, two-word aggregate budget, per-object rounding boundary, incomplete and
cv-qualified expressions, over-alignment, oversize, owning-versus-reference `parameter`, precise-prefix shape, and the
exact-two/one-word context boundary. Its runtime cases verify the marked nine-proxy hot and short-buffer paths, the
owned context pair, the large and unmarked reference fallbacks, a deleted-copy alias reference, and a move-only status
observer. The broader scan concept, mutex, and protocol matrices verify that status, context, and unlocked-source
selection preserve their established semantics.

Three deterministic randomized drivers were also built with address and undefined-behavior sanitizers on Linux x86-64
and invoked only through the silent wrapper. All returned status `0` (`concept_composition_rc=0`,
`line_scanner_contiguous_rc=0`, and `semantic_composition_rc=0`). This is correctness evidence only; randomized-driver
output and runtime are not benchmark data.

## scnlib comparison boundary

The comparison model here follows scnlib v4's official public documentation and the v4.0.1 source, not assumptions
about its numeric conversion internals:

- A user type specializes `scn::scanner<T, CharT>` and provides `parse(ParseContext&)` plus
  `scan(T&, Context&)`. `parse` interprets format specifications and stores any resulting scanner state; it is intended
  to be `constexpr` so format-string checking can occur during constant evaluation.
- `scan` receives a range-aware context and returns an expected iterator/error result. On success, that iterator is
  past the consumed input. The scan context supplies its current range, `begin`, `end`, `advance_to`, argument access,
  and the scanner type associated with a target.
- The parse context owns format-string iteration and argument indexing. Its compile-time derivative carries the
  additional state needed for format checking.

Official references:

- [scnlib guide: user-defined types](https://www.scnlib.dev/guide.html#user-types)
- [`scn::scanner` reference](https://www.scnlib.dev/structscn_1_1scanner.html)
- [`scn::basic_scan_context` reference](https://www.scnlib.dev/classscn_1_1basic__scan__context.html)
- [`scn::basic_scan_parse_context` reference](https://www.scnlib.dev/classscn_1_1basic__scan__parse__context.html)
- [scnlib v4.0.1 public header source](https://github.com/eliaskosunen/scnlib/blob/v4.0.1/include/scn/scan.h)

The architectural difference is therefore descriptive, not a performance conclusion. fast_io exposes several
independent ADL capabilities and chooses a source-dependent precise/contiguous/context/status strategy. scnlib places
format-spec parsing and value scanning in a scanner object and gives that object a range-aware scan context with an
expected result. Either design can perform different amounts of public-API work for the same apparent token. A fair
measurement must state which work is included.

## Fair benchmark procedure

For any future fast_io/scnlib comparison:

1. Use identical byte sequences and grammar: fixed records, literal matching, or one delimiter-terminated text field.
   Do not use built-in integer or floating scanners in this directory.
2. Match input ownership and chunk boundaries. Compare terminal ranges separately from a refill/file scenario; do not
   compare fast_io's terminal contiguous path with a refill-aware context path on the other side.
3. Match observable semantics: consumed iterator, delimiter policy, EOF acceptance, target commit, and error result.
4. Report at least two measurements: a scanner-kernel/call-barrier case and the public end-to-end API. The latter may
   include scnlib format parsing/checking and expected-result machinery, or fast_io alias/forward/source dispatch; do
   not silently subtract work from only one side.
5. Keep compiler, standard-library, optimization, target ISA, LTO setting, warm-up, iteration count, and CPU affinity
   identical. Record generated code size and compile time separately when they are relevant; neither is runtime.
6. Retain the noinline `fake_observe` baseline. It exposes common call and compiler-barrier cost without pretending to
   model a read syscall. A real I/O benchmark belongs in a separate suite with the same syscall and buffering policy.
7. Publish raw samples and dispersion, not only the fastest sample. Do not infer that a concept architecture is faster
   from a result in which one side parsed a format string, performed Unicode validation, allocated, or returned richer
   error state while the other side did not.
8. On Linux, resample whole-machine and per-CPU occupancy before every batch, then pin one timing process to one idle
   logical CPU of an otherwise idle physical P-core. P-core and E-core use is opportunistic and capped at 60% of each
   class; yield when another workload appears. Pin builds, correctness tests, and the silent random-test wrapper to
   idle E-cores. Cross-class activity is allowed, but record it and reject a timing batch whose alternating samples
   show abnormal dispersion; package-power and memory traffic can perturb a nominally idle target core. Prefer a
   no-compilation confirmation when practical. On Apple M4, use exactly one process/thread.

## Isolated scan-prefetch policy probe

`scan_prefetch_probe.cc` tests two prefetch *sites* without modifying the scan dispatcher or a numeric conversion
algorithm. In `read` mode, both kernels call the same noinline branchy token consumer; the candidate adds exactly one
bounded L1/keep read hint before that call. In `refill` mode, both kernels perform the same `read` from `/dev/zero` into
the destination; the candidate adds exactly one bounded L1/keep write hint. The two directions are intentionally not
combined because a producer write and a later parser read have different instruction lowering and cache costs.

`hot` mode reuses one allocation, matching the steady state of an owned input buffer. `cold` mode visits a shuffled
ring larger than the measured machine's last-level cache. Cache scrubbing occurs outside each timed interval, sample
order alternates, and equal checksums are required before a pair is accepted. `distance < extent` is checked before
forming the hinted address, so the probe never relies on a one-past or unrelated address. The executable reports
candidate/baseline paired medians; values below one favor the hint.

Build and run it only in the remote Linux `/tmp` tree, after selecting an idle CPU:

```sh
g++-15 -std=c++23 -O3 -march=native -Iinclude \
  benchmark/0020.scan_concepts/scan_prefetch_probe.cc -o scan-prefetch-probe

taskset -c <idle-p-cpu> ./scan-prefetch-probe \
  --operation read --cache cold --extent 16384 --distance 1024 \
  --target-bytes 268435456 --cold-bytes 134217728 --scrub-bytes 67108864 \
  --samples 11 --warmups 3 --seed 11400714819323198485
```

The probe is an admission test, not a request to emit an instruction. A positive cold-only result does not authorize a
site used on a hot reused buffer, and instruction availability does not replace explicit source/destination provenance,
live in-range bounds, run-time thresholds, or a constant-evaluation no-op.
