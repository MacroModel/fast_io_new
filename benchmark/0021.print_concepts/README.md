# Print concept-composition benchmark

The main composition harness isolates formatting and output-concept composition. It intentionally contains no
integer/floating-point formatting and no numeric parsing. The printable leaves are strings plus a fixed five-byte
custom reserve-printable token, so width, condition, pack, range-view, concat, buffering, scatter, and file-reference
policies can be compared without attributing a conversion algorithm's cost to the concept layer.

## Runtime floating scatter crossover

`runtime_float_scatter_bench.cc` is the separate numeric transport calibration. Every iteration formats an optimizer-
opaque `double` as shortest general, fixed, or fixed with a run-time precision, places that result among 2/3/5/9
nonempty descriptors, and sends the complete record to a real unbuffered `/dev/null` descriptor. Before timing, the
copy-plus-write and native-writev spellings are materialized independently and compared byte for byte. The
`print_policy` cases then exercise the library's
`scatter_direct_full_output_coalesce_threshold` selection on the same records.

Google Benchmark medians on the fixed i9-14900HX P-core CPU 4 are summarized below as
`(copy + write) / writev`; values below one favor contiguous materialization. Each cell is the range across all three
floating modes and all four descriptor counts. The crossover run used seven repetitions per case; GCC's median CV was
0.72% (90th percentile 1.74%), while Clang's median CV was 1.26% (90th percentile 3.57%). A few isolated Clang samples
were noisier, but the complete 4-KiB band remained on the same side of the crossover.

| Compiler | 2 KiB | 4 KiB | 8 KiB |
|---|---:|---:|---:|
| Clang 23 | 0.626--0.774 | 0.754--0.926 | 0.880--1.153 |
| GCC 15 | 0.645--0.826 | 0.730--0.919 | 0.910--1.153 |

At 4 KiB, copying was 7.4%--27.0% faster in every measured case. At 8 KiB, 2/3/5-descriptor records had crossed to
writev for every floating mode; only the nine-descriptor cases still repaid copying. Because the stream policy has one
payload threshold rather than a descriptor-count-dependent table, 4 KiB is the largest uniformly profitable measured
Linux default. The benchmark therefore confirms the existing 4-KiB concept value and tests the missing strategy
application instead of replacing it with a guessed constant. Small records now complete through one direct scalar
write; a record above the policy threshold retains native scatter output, and a destination with a put area bypasses
the unbuffered policy entirely.

One reproducible build and run is:

```sh
make -C benchmark/0021.print_concepts runtime-float-scatter \
  CXX=clang++ GOOGLE_BENCHMARK_ROOT=/path/to/google-benchmark \
  GOOGLE_BENCHMARK_BUILD=/path/to/google-benchmark-build
taskset -c 4 benchmark/0021.print_concepts/runtime_float_scatter_bench \
  --benchmark_out=/tmp/runtime-float-scatter.json --benchmark_out_format=json
```

## Static precision floating width on a put area

`static_precision_float_width_bench.cc` isolates the runtime-unknown `float`/`double` case whose width and precision
are encoded in the compiled format program, including `{:+020.6f}`.  It uses a reusable 2-KiB `obuffer_view`, reads
every timed value from a volatile source, and keeps the written storage observable with an inline-assembly memory
operand.  Before timing, each spelling is compared byte for byte with the corresponding `concat_std` result.  The
matrix includes internal/right/left/center placement, insufficient width, a signed prefixed hex field, no width, and
both right/internal dynamic-width-and-precision controls.

The optimized internal-placement path is deliberately narrower than an ordinary `reserve_printable` test.  A field
must already carry the explicit bounded-materialization and direct-put-area markers, its non-fatal candidate bound and
actual reserve define must be non-throwing, and the independently forwarded plain leaf must provide a non-throwing
internal-shift CPO.  Conditions, packs, nested widths, and an unmarked or potentially throwing custom leaf retain the
measured fallback.  `tests/0003.concat/static_precision_float_width_put_area.cc` exercises that negative gate, counts
one bound/define/shift call on each admitted internal field, and compares finite, subnormal, extreme, infinity, NaN,
negative-zero, prefix, and placement cases.

Build and pin the focused benchmark with:

```sh
make -C benchmark/0021.print_concepts static-precision-float-width \
  CXX=clang++ GOOGLE_BENCHMARK_ROOT=/path/to/google-benchmark \
  GOOGLE_BENCHMARK_BUILD=/path/to/google-benchmark-build
taskset -c 4 benchmark/0021.print_concepts/static_precision_float_width_bench \
  --benchmark_out=/tmp/static-precision-float-width.json \
  --benchmark_out_format=json
```

`static_precision_float_width_asm.cc` supplies non-null 2-KiB wrappers for code-shape inspection.  In the ample-put-area
branch, an admitted static internal field must call its floating reserve define once, then move/fill padding after the
reported sign/prefix.  `floating_precise_precision_size` and the measured width continuation may remain in the same
object, but only behind the null/short-capacity fallback; counting calls in the complete object without following the
branch would therefore be misleading.  The dynamic width/precision wrapper is the control for the same one-pass
property.

```sh
make -C benchmark/0021.print_concepts static-precision-float-width-asm CXX=clang++
objdump -drC benchmark/0021.print_concepts/static_precision_float_width_asm.o
```

## Run-time source-bridge inlining tradeoff

`compiler_constant_bridge_outline_bench.cc` compares the ordinary-inline
source-normalization bridge with an otherwise identical header copy in which
only `print_freestanding_decay_unforwarded` regains its former forced-inline
attribute.  It measures an optimizer-unknown integer, an optimizer-unknown
fixed `double`, and random indirect calls across 128 distinct compiled format
strings, all writing to a reusable `obuffer_view`.

Ten alternating GCC 15 runs pinned to the i9-14900HX P-core CPU 4 used 0.10 s
per case.  The paired median `forced / ordinary` CPU-time ratios were 0.988 for
the integer, 0.902 for the floating field, and 1.043 for the 128-format working
set.  Thus forced inlining bought about 9.8% in the isolated floating call, but
lost about 4.3% under the larger instruction working set; the integer result
was within about 1.2%.  More importantly, the 128 generated wrappers occupied
196,352 bytes with forced inlining versus 14,976 bytes with ordinary inlining
(13.1x), while total executable text grew by 20.43%.  The bridge therefore
remains ordinary inline: the compiler may still inline a small isolated use,
without cloning the complete dynamic formatter into every format program.

Build the current side with:

```sh
make -C benchmark/0021.print_concepts compiler-constant-bridge-outline \
  CXX=g++-15 GOOGLE_BENCHMARK_ROOT=/path/to/google-benchmark \
  GOOGLE_BENCHMARK_BUILD=/path/to/google-benchmark-build
taskset -c 4 \
  benchmark/0021.print_concepts/compiler_constant_bridge_outline_bench
```

For an A/B run, compile this same source and flags against a copied header tree
whose only difference is the one bridge attribute, and alternate process order
on the same pinned core.

## Static floating arguments with width and precision

`static_argument_fixed_width_asm.cc` verifies the variable-template spelling
`mnp::static_arg<12.44>` through both brace and printf programs.  The matrix
contains zero-filled internal placement and center placement, plus an ordinary
run-time `double` control. Every static wrapper must pass one complete 24-byte
record from the IO planner's `print_static_provider_merged_run_provider`
storage in `.rodata` to exactly one direct syscall, with no stack allocation,
formatter call, or scatter path. The run-time control must not acquire either
the merged-static-provider or compiler-constant machinery.

The gate checks every available GCC 11--16 and Clang 17--23 executable, compares
the complete output bytes, and inspects the linked code and symbol table:

```sh
TASKSET_CPU=26 \
CLANG23_CXX=/path/to/clang++-23 \
make -C benchmark/0021.print_concepts static-argument-fixed-width-asm-gate
```

## Static-argument large records and the storage boundary

`static_argument_large_record_asm.cc` checks 65-byte, 4-KiB, and 64-KiB
`mnp::static_arg` strings. The last case is the current core static-provider
semantic budget, not an unbuffered transport threshold. Each complete static
replacement owns its final code units in one provider-owned core freestanding
array in `.rodata`; all three sizes pass
that provider directly to one scalar write retry loop with no stack record,
formatter call, or scatter metadata.  A complete type-level static provider is
therefore not capped by the 64-byte automatic-record policy or the 4-KiB
run-time copy/writev crossover.

The corresponding limit probe is intentionally also a compiler-resource gate.
On this tree its measured peak front-end RSS was 347,916 KiB for GCC 13,
356,040 KiB for GCC 15, and 317,996 KiB for Clang 23.  The script checks all
available GCC 11--16 and Clang 17--23 executables and allows 768 MiB by default;
`FAST_IO_STATIC_ARGUMENT_MAX_RSS_KIB` can select a stricter build-machine
budget.

```sh
make -C benchmark/0021.print_concepts static-argument-large-record-asm-gate
```

This guarantee requires the value to be represented in the template/type
graph.  A plain call such as `fmt::print<"i = {}">(out(), 32)` shares its
function-template specialization with every other run-time `int`, so C++20
cannot use that parameter to initialize a value-dependent static object.  When
the optimizer proves such a short ordinary argument, the print layer instead
builds one caller-local core array and performs one scalar write. In contrast,
`mnp::static_arg<32>` permits a merged provider-owned core freestanding array
in `.rodata`. A single string
literal needs neither construction: its existing literal pointer is forwarded
directly.

The fast_io-only protocol workloads extend that matrix with pure-text dynamic-reserve (`dynamic9`), retained static
reserve-scatters (`reserve-scatter9`), incremental context (`context3`), and staged prepare/emit (`staged9`) producers.
Named width cases cover left, middle, right, and semantic internal-shift placement; the latter has no equivalent string
alignment operation in fmt and therefore remains fast_io-only. These cases compare strategy changes between the frozen
and current fast_io header trees; protocol emulation on fmt would measure a different public abstraction.

Each invocation runs exactly one backend/operation/workload tuple. Before the clock starts, `print` and `println` cases
are rendered into an independent capture sink and compared with a manually constructed reference. Concat and reusable
buffer results are compared after the clock stops. The reported byte count and FNV-1a checksum are therefore computed
outside the timed interval. No benchmark case writes to the terminal; real file cases target `/dev/null`.

`fake-only` invokes only the noinline, compiler-barrier-protected scalar boundary and is the pure opaque-call baseline.
`fake-write` runs the complete print composition before that same boundary but the boundary itself performs no copy.
`fake-scatter` provides scalar and native-scatter boundaries with a realistic finite descriptor limit. `obuffer`
reuses one contiguous buffer and therefore measures dispatch plus actual memory writes. Owner/observer pairs use the
same native resource but enter the library through different object concepts.

`large-fake-observer` and `large-obuffer-observer` are the targeted transport controls. Each customization normalizes a
128-byte non-trivial observer into one owned proxy whose copy/move constructors increment shared state. The former ends
at the same noinline compiler barrier as a fake syscall; the latter writes into a reusable true put area. Their output
adds `copies/op`: one is the explicit normalization contract, while every excess copy comes from recursive dispatch.
The object is deliberately outside the known register-aggregate envelopes, but size is not used as the semantic
identity proof; its non-trivial copy operation is why the owned proxy must remain borrowed after normalization.

The stable-reference case is a separate capability proof. Current public entry removes reference qualification before
reading stream traits and then preserves a large CPO lvalue result; focused ownership tests require exactly zero copies.
The frozen entry could not form that expression, so pretending to time it would compare a supported current protocol
with an ill-formed baseline rather than two equivalent operations.

`static_literal_asm.cc` isolates compile-time-known text materialization. The split probe spells
`"a", "bbb", chvw('c')`; the control spells `"abbbc"`. Both reach one noinline compiler-barrier-protected scalar write,
so the exported wrappers contain no syscall, checksum, allocation, or numeric conversion. Before the static-scatter
extent remained a run-time helper parameter, GCC 15 reconstructed an independent three-byte `memcpy`: the split
spelling used four payload stores while the control used two. The fixed-extent helper now expands `N <= 16` as indexed
assignments before loop-to-memcpy recognition. Both forms consequently lower to one dword store plus one byte store on
x86-64; GCC 15 on M4 and Apple Clang likewise give the two wrappers the same payload instruction shape. Build and
compare the complete symbols:

```sh
g++ -O3 -march=native -std=c++20 -Iinclude \
  benchmark/0021.print_concepts/static_literal_asm.cc -o /tmp/static-literal-asm
objdump -drC /tmp/static-literal-asm | \
  sed -n '/<fast_io_static_literal_split_probe>/,/^$/p; \
          /<fast_io_static_literal_whole_probe>/,/^$/p; \
          /<fast_io_static_literal_line_probe>/,/^$/p; \
          /<fast_io_static_literal_embedded_line_probe>/,/^$/p'
```

`compiler_constant_float_posix_asm.cc` is the GCC 15 gate for a complete short constant record on an unbuffered
POSIX observer.  The print layer joins `"i="` and `3.2` in its eight-byte scratch tier and enters the direct scalar
write CPO in the normal path.  After disabling toolchain-default stack/control-flow hardening to expose only library
cost, `main` must contain the merged dword/byte payload stores and one inline `write` syscall retry loop: it must not
adjust the stack, call a cold write helper, or use scatter/writev.  GCC may partition the syscall-error `ud2`; the gate
allows that block only when it has no call, syscall, or normal return.  Run the executable assembly check with:

```sh
make -C benchmark/0021.print_concepts compiler-constant-float-asm-gate
```

`format_plain_float_lowering_asm.cc` is the complementary Clang 23-and-newer
lowering gate for a plain `{}` float/double field.  It classifies exported
symbols and call targets instead of matching a particular immediate-store
sequence: constant leaves must reach one scalar transport (an inline syscall or
one recognized outlined direct-write transfer) with no `format_scalar_t`
forwarding CPO or run-time floating conversion.  A separate compact-leaf gate
bounds the exported ELF symbol size, instruction count, and static stack frame;
this rejects a complete conversion engine even if a future optimizer inlines it
and thereby removes every recognizable call target.  An
optimizer-unknown leaf may either inline the native converter or call the core
`scalar_manip_t` reserve leaf once.  Both runtime shapes must remain free of the
format identity wrapper, compiler-constant proxy state, and scatter output.
Compile-time lowering assertions additionally prove that precision, alternate
form, sign, width, and e/f/g/a presentation still retain their semantic types;
the native leaf exemption is restricted to grammar-empty `{}`.  Every compiler
is checked in both C++20 and the exact C++26 mode that exposed the Clang-trunk
regression.
The script intentionally skips non-Linux-x86_64 targets because its syscall and
ELF/Itanium symbol checks are platform-specific.  It tests an installed Clang
23 and additionally `CLANG_TRUNK_CXX` when supplied:

```sh
make -C benchmark/0021.print_concepts format-plain-float-lowering-asm-gate
CLANG_TRUNK_CXX=/path/to/clang++ \
  make -C benchmark/0021.print_concepts format-plain-float-lowering-asm-gate
```

## Static-fragment first-attempt placement

`static_fragment_direct_write_bench.cc` compares the established outlined
two-iovec completion, a caller-local first `writev`, a six-byte stack copy plus
`write`, and an explicit `mnp::static_arg<32>` whose complete spelling is one
provider-owned DSAL array.  On CPU 4, 25 alternating `/dev/null` process pairs
gave the public hot-first path median changes of -5.44% on GCC 15 and -4.31% on
GCC 13.  Clang 23 was neutral at the median and +2.26% slower by paired mean.
The placement policy is therefore intentionally GCC-only; the synchronous
direct-output protocol and provider-lifetime gate remain compiler-independent.

`check_static_fragment_hot_policy_asm.sh` locks that division down: available
GCC 13--16 builds must place the first syscall in the probe and leave only
partial/error completion cold, while available Clang 17--23 builds must retain
the outlined cold helper.  Run it with:

```sh
make -C benchmark/0021.print_concepts static-fragment-hot-policy-asm-gate
```

This is deliberately an ordinary constexpr copy policy, not consteval string concatenation. The public function's
literal values cannot be promoted to C++20 non-type template arguments, and a general reserve CPO may have effects or
return less than its advertised upper bound. `static_scatter_t<N>` carries the required type-level extent contract:
`N <= 16` uses an index expansion for cross-fragment merging, while `N > 16` retains the memcpy-shaped path. Constructing
the scatter still requires its base to denote at least N readable elements. General run-time scatters keep the separately
measured bounded-copy policy. The correctness suite uses sources containing exactly N live elements (no readable
terminator padding), covers 1/3/16/17 and multiple character widths, and exercises both ordinary and precise reserve CPOs.

Line ownership preserves the same type-level materialization opportunity. The line probe calls the public
`println(sink, "abc")` entry; its control calls public `print(sink, "abc\\n")`. Both own one four-character reserve
window and make one scalar boundary call. GCC 15 emits the complete little-endian payload as one
`movl $0x0a636261,(destination)` in each wrapper. Apple Clang on M4 emits the same literal-pool load and one 32-bit
store in each wrapper, with otherwise identical instruction sequences. With stack and control-flow hardening disabled,
the two GCC 15 exported symbols are both 32 bytes. No special-case literal construction is needed:
the line policy contributes one checked slot to the fixed reserve capacity, `static_scatter_t<3>` exposes the first
three assignments, and the adjacent newline assignment remains visible to ordinary store merging. This proof does not
extend to a run-time scatter or an arbitrary formatter, whose length, effects, or destination call boundaries are not
type-level facts. Runtime regressions compare line-owned and embedded-newline output at 1/3/15/16/17 payload boundaries
for all supported character widths and require one completed scalar range in both forms.

On Linux x86-64, GCC 15 produced 37-byte clean and 75-byte default-hardened exported symbols for both spellings; the
complete instruction sequences, including the byte store for `c` and dword store for `abbb`, were identical. In the
final frozen-source gate, eleven alternating 100-million-call samples on an opportunistically idle physical P-core
measured 1.780192 ns/op for the split form and 1.779746 ns/op for the control (median difference +0.025%, paired-ratio
median -0.269%). This is indistinguishable from layout/timing noise and confirms that the concept-level composition no
longer adds a measurable run-time penalty. The matching timed probe
accepts `s` for the split spelling and `w` for the whole-literal control; alternate their order across samples rather
than running every copy of one variant first:

```sh
g++ -O3 -march=native -std=c++20 -Iinclude \
  benchmark/0021.print_concepts/static_literal_bench.cc -o /tmp/static-literal-bench
taskset -c 5 /tmp/static-literal-bench s
taskset -c 5 /tmp/static-literal-bench w
```

Compiled scatter plans retain a different proof boundary. Direct calls normalize each used public argument once;
bound calls first retain an ABI-decayed pre-alias source and recreate its alias only during synchronous emission. The
latter ordering is necessary for an owning rvalue string whose alias points into its own SSO or heap storage: storing
only the alias would outlive the source factory expression. Repeated dynamic components materialize the first
descriptor once and copy it into later component positions. Their reuse map is component-sized compile-time data,
generated by a stable fixed-width radix pass. It has deterministic O(N) constant-evaluation work, no recursive
template search, and no runtime descriptor-cache storage proportional to a sparse public argument index.
Regression sources cover mutable-only scatter CPOs, saved rvalue self-buffer aliases under ASan, legal argument holes,
move-only transports, and 64 repetitions with one observable CPO invocation.

`observer_transport_asm.cc` isolates that distinction from the large composition translation unit. It exports one
stable-reference probe and one intentional owned-proxy probe for the same 128-byte non-trivial observer shape. Both
terminate at a noinline compiler barrier. The stable probe must return zero and contain no copy-constructor call; the
owned probe must return one and contain exactly the normalization copy, with no second copy in print dispatch. Compile
and inspect the two exported symbols with the same optimizer and target flags used by the runtime benchmark:

```sh
g++ -O3 -march=native -std=c++20 -Iinclude -c \
  benchmark/0021.print_concepts/observer_transport_asm.cc -o /tmp/observer-transport.o
objdump -drC /tmp/observer-transport.o
```

`reserve_scatter_asm.cc` is a code-shape probe for the grouped reserve-scatters byte adapter. Its sink exposes native
byte-write and byte-scatter boundaries whose noinline bodies are compiler barriers only. Each pure-text producer
contributes three borrowed descriptors and zero reserve bytes, so the exported
`fast_io_reserve_scatter_byte_adapter_probe` symbol contains descriptor planning/adapter work but no payload copy,
checksum, allocation, syscall, or numeric formatting. `FAST_IO_RESERVE_SCATTER_NOINLINE_PRODUCER` models a CPO from a
separate translation unit; `FAST_IO_RESERVE_SCATTER_NATIVE_BYTES` enables the exact native-byte refinement, and
`FAST_IO_RESERVE_SCATTER_TOKEN_COUNT=N` selects the composition width. Build both paths separately:

```sh
clang++ -O3 -march=native -std=c++20 -DNDEBUG \
  -DFAST_IO_RESERVE_SCATTER_NOINLINE_PRODUCER \
  -DFAST_IO_RESERVE_SCATTER_TOKEN_COUNT=9 -Iinclude \
  benchmark/0021.print_concepts/reserve_scatter_asm.cc -o /tmp/reserve-scatter-asm
clang++ -O3 -march=native -std=c++20 -DNDEBUG \
  -DFAST_IO_RESERVE_SCATTER_NOINLINE_PRODUCER \
  -DFAST_IO_RESERVE_SCATTER_NATIVE_BYTES \
  -DFAST_IO_RESERVE_SCATTER_TOKEN_COUNT=9 -Iinclude \
  benchmark/0021.print_concepts/reserve_scatter_asm.cc -o /tmp/reserve-scatter-native-asm
llvm-objdump -d --disassemble-symbols=fast_io_reserve_scatter_byte_adapter_probe \
  /tmp/reserve-scatter-asm
```

The probe is assembly evidence, not a runtime benchmark. Token lengths are populated at run time and cross the
noinline adapter boundary; with the producer also noinline, the caller can rely only on the CPO contract and cannot
scalar-replace its descriptor writes. The portable path must therefore materialize a genuine typed descriptor array,
validate it, and convert each live descriptor member-wise. The optional native CPO writes genuine `io_scatter_t`
objects with byte lengths and removes that array and conversion without claiming an effective-type exception. The
ordinary composition harness must still measure real run-time behavior and C-file families. When comparing revisions,
report the complete reachable call graph as well as the exported wrapper: an older strategy may leave its planner out
of line, making wrapper size alone misleading.

Supporting Apple M4 code-shape evidence (Clang, `-O3 -march=native`, opaque producer) shows why the refinement exists;
these are deterministic instruction/stack properties, not the final opportunistically pinned Linux timing result:

| Producers | Portable wrapper bytes | Native wrapper bytes | Portable/native stack bytes |
|---:|---:|---:|---:|
| 1 | 340 | 168 | 160 / 144 |
| 9 | 2,916 | 808 | 608 / 560 |
| 33 | 10,600 | 2,636 | 1,760 / 1,712 |

The stack delta is one reusable three-descriptor typed scratch array; the code delta grows with producer count because
the portable adapter must validate and copy every returned descriptor. A `char16_t` regression additionally proves
that native lengths are bytes while the reserve cursor remains in character units, including the `println` newline.

## Retained reserve-scatters on a true put area

A leading adjacent run of at least two static reserve-scatters producers may use one bounded plan on a destination
with real put-area cursors. Each component contributes compile-time descriptor and reserve-storage capacities. The
implementation invokes `print_reserve_scatters_define` exactly once per component, in source order, validates both
returned cursors against that component's own closed ranges before invoking the next producer, and finally submits the
complete live descriptor prefix through the established scatter dispatcher. A fitting put area is therefore acquired
and committed once, while the existing dispatcher still owns short-buffer fallback and native-scatter selection.

`borrowed_reserve_scatters_source` is only the retention proof for that plan: descriptors produced by one component
remain valid across later producer calls and the final synchronous drain. It does **not** promise producer purity,
repeatability, replay safety, or stable output from a second invocation. The buffered strategy consequently never
premeasures by calling `define`, never replays a component, and does not admit reserve-scatters leaves into the semantic
precise-size machinery. The exact lvalue `define` call must also be `noexcept`; an unmarked or potentially throwing
producer retains the established immediate per-leaf path.

Descriptor and reserve arrays share one stack-byte budget, clamped by the central stack policy and a local 4-KiB
ceiling. Runs outside that cost envelope fall back unchanged. There is intentionally no permanent benchmark-only
macro for disabling this strategy. For a same-source A/B control, insert a temporary `return false;` as the first
statement of `print_retained_buffered_reserve_scatters_run_selected`, rebuild the candidate with identical flags, and
remove the one-line scratch edit after collecting the alternating samples.

Repeated large scatter leaves expose a genuine size/throughput tradeoff on that true-put-area path. With four explicit
`basic_io_scatter` leaves, reachable optimized text grew as follows relative to one leaf; the figures include shared
cold code where the compiler emitted it.

| Payload per leaf | GCC 15, four / one | Clang 21, four / one |
|---:|---:|---:|
| 128 bytes | 528 / 195 bytes | 615 / 91 bytes |
| 256 bytes | 768 / 83 bytes | 915 / 91 bytes |

That growth is not sufficient evidence for a default size gate. On the same opportunistically selected physical
P-core, forcing the four-leaf path through the established fallback was 16.1% slower at 128 bytes and 8.2% slower at
256 bytes. Outlining every leaf was 170.6% and 43.1% slower respectively, and Clang's complete reachable outlined
graph grew to roughly 1,982 bytes. Isolated fake-boundary controls produced identical binaries and approximately
0.900 ns/op across strategies, showing that earlier fake-call differences were layout noise rather than saved work.
The throughput-first direct materialization therefore remains the default. A future cold/size refinement needs an
explicit destination or policy concept and one batch helper; per-leaf noinline and an implicit payload threshold are
not supported by these measurements.

`ostring-std` and `ostring-fast` reuse pre-reserved standard/fast_io strings through their respective output-reference
objects. `obuf-owner` passes the buffered file owner through normal API normalization, whereas `obuf-ref` hoists the
exact normalized reference outside the timed loop. Locked (`c-owner`/`c-observer`) and unlocked C-file pairs are kept
separate because libc locking is part of the former object's contract.

## Evidence-backed width ownership boundary

A single plain, statically bounded width node must remain in the outer bounded coalescer. Merely discovering an
`obuffer` is not evidence that the checked direct-width dispatcher is cheaper. The direct route is reserved for a
width whose run-time-scatter or semantic child already owns a descriptor/branch traversal; only then would outer
materialization repeat work.

On an otherwise idle Linux physical P-core, seven alternating samples gave the following medians (ns/op). `gated` is
the ownership predicate used by the working headers; all cases passed exact output-length and byte checks.

| Existing put area | Width | Frozen baseline | Over-broad direct gate | Ownership gate |
|---|---:|---:|---:|---:|
| left | 257 | 3.821 | 7.747 | 3.712 |
| left | 4097 | 17.217 | 45.555 | 17.395 |
| middle | 4097 | 25.007 | 61.476 | 24.778 |
| right | 4097 | 21.201 | 43.900 | 20.812 |
| internal | 4097 | 17.594 | 44.817 | 17.202 |

The assembly explains the complete delta. For compact fields, the over-broad route formats in a stack window and then
copies the complete field into an already-large-enough put area. For a 4097-character left field, its large-fill helper
builds a 4-KiB stack block, fills it, and copies 4092 padding bytes to the destination. The ownership-gated outer path
first proves `end - current >= max(width, child_bound)`, writes the five-byte child in the final destination, and fills
that destination once. Middle/right retain only the necessary five-byte move; internal placement writes around the
proved shift point. This is a concept-strategy failure, not a fill-kernel failure, so no numeric or formatting algorithm
was changed.

## Current normalization and ownership contracts

The public print boundary performs character-aware alias/status forwarding and decay exactly once. The resulting
objects are deliberately a small, optimizer-visible set: compact values are owned by the decayed entry and an
identity-sensitive lvalue is represented by `parameter<T&>`. The owning entry then calls a stable-reference dispatcher;
all deeper strategy selection observes those same objects by reference. This is a lifetime and type-category contract,
not a run-time cost guess.

Mutex and cold paths preserve that ownership boundary. A mutex wrapper is locked before status or output-specific
selection, its unlocked observer is materialized once in a local object, and recursive dispatch retains references to
the original normalized arguments. The cold wrapper likewise owns its normalized arguments once and enters the same
reference dispatcher. Consequently neither path reconstructs a width/condition/pack graph, and a graph that owns a
move-only leaf is not made ill-formed by an incidental dispatcher copy.

Recursive formatters have a related but distinct entry. A range `print_define` already receives the exact output
observer chosen by the enclosing operation, so it calls `print_freestanding_decay_unforwarded`: that bridge normalizes
only each source element and never asks for an output reference of the already-normalized output reference. This proves
that a non-idempotent observer is not wrapped twice. It also keeps iterator-proxy temporaries inside the synchronous
nested call instead of manufacturing a reference that could escape the element expression.

Locale output follows the same one-owner rule. `imbue` preserves a normalized mutable lvalue observer as a reference
and owns any value result once; its status continuation, mutex-unlocked recursion, semantic binding, and direct-print
bridges then borrow that named object. Locale `parameter`, condition, and nested-pack adapters likewise borrow the
already-owned formatter graph. This matters beyond copy cost: a locale scatter may point into an owning branch, and a
by-value adapter would return a descriptor into its destroyed local copy. Locale retained-scatter opt-in also promises
repeatability, not only pointer lifetime. The built-in alphabet-boolean and AM/PM sources make that exact promise after
resolving their relative locale descriptors; scratch or alternating stable-buffer producers remain unmarked.

Relative locale descriptors now resolve through an explicit storage link in `basic_lc_all`. The complete
`basic_lc_object` rebinds that link after construction, copy, move, assignment, and swap; moved-from objects are reset
to coherent empty facets. This replaces a rejected container-of implementation: `basic_lc_object` contains
`std::vector`, which the C++ standard does not require to be standard-layout, so subtracting `offsetof(all)` from a raw
facet pointer was not a portable object-model proof. Resolution also checks the relative interval before pointer
arithmetic. The historical one-pointer locale CPO ABI is unchanged; the cost is one owner-pointer load, while copying a
standalone `basic_lc_all` remains an explicitly documented borrowed view of the original storage.

## Put-area ownership and range strategy

`buffered_printable_preferred` and `put_area_printable_preferred` express different destination costs. The former may
describe an amortized append adapter; the latter applies only when an output exposes the exact writable put-area cursor
protocol. A fixed-reserve sized range, and a sized range of explicitly borrowed/repeatable character scatters, use the
narrow put-area marker. `parameter<T&>` forwards an existing marker but cannot create one. The destination-side gate
independently requires real obuffer operations and a `print_define` callable for that exact observer. Thus an
append-only string adapter cannot inherit a cost decision established for a true put area.

The range protocols make the semantic proofs explicit:

- A fixed-reserve element type supplies a per-element compile-time upper bound. A sized or contiguous single-pass input
  range can therefore use that count without a measuring traversal. On a true put area, the range may emit elements
  incrementally; otherwise the ordinary contiguous reserve path remains canonical.

- Object-dependent reserve sizes require a forward iterator before a sized, two-pass materialization is admitted.
  Iterator multipass alone is not enough for scatter elements: the original dereferenced source must be an lvalue and
  its type must opt into the borrowed/repeatable scatter contract. Alias result shape cannot manufacture that lifetime
  proof.

- An eligible borrowed raw-character-scatter range reports a run-time descriptor capacity of at most one element
  descriptor plus one nonempty separator descriptor between adjacent elements. It requires no reserve-character
  storage. Checked arithmetic proves the capacity, while the returned cursor identifies the actual prefix when empty
  components remove descriptors. Large plans may use dynamic descriptor storage; this changes storage placement, not
  ordering or lifetime requirements.

- A narrower direct put-area refinement is available only when three semantic proofs compose. The element source must
  state both that alias/forward/scatter observation is independent of every output cursor and that its direct scatter
  is observationally equivalent to the source's element and separator/element print semantics. The latter excludes a
  hidden source-associated status hook whose effects would be bypassed by raw copying. The destination must state that
  its put area remains stable, that intermediate cursor publications may be folded, and that its own status/locking
  vocabulary does not alter a separately marked source's direct-scatter semantics. `noexcept` is not a substitute for
  any of these promises. The implementation additionally requires a raw-pointer iterator, so iterator operations cannot
  hide callbacks, and enters the out-of-line loop only at sixteen elements. A capacity miss publishes the copied
  prefix, reuses the already-observed boundary descriptor, and resumes through the ordinary output bridge. Thus the
  source is never restarted merely because the put area is short.

  The three source proofs are also specialization-exact. `std::basic_string` is opted in only with the standard
  character traits and allocator, `std::basic_string_view` only with standard traits, and fast_io string only with its
  two native allocator models. Traits and allocator template arguments contribute associated namespaces to ADL; a
  user namespace can replace an otherwise contiguous type's alias/forwarding protocol with shared scratch or stateful
  behavior. Contiguity therefore cannot justify a blanket marker for those extensible specializations. Negative tests
  install exact ADL aliases for all three extension points and prove that they remain on the conservative path.

  `io_strlike_reference_wrapper` does not infer either destination policy from structural string-like syntax. The
  underlying `T` must separately opt into amortized buffered printing and, for cursor folding, the stronger deferred-
  commit semantic contract. Only the internal concat buffer, default-traits/default-allocator standard string, and the
  two native fast_io string allocator families currently supply those audited proofs. A custom allocator or user ADL
  output hook therefore falls back unless its implementation explicitly accepts the corresponding contract.

- Ineligible, unsized, proxy-producing, or single-pass object-dependent ranges retain the streaming/contiguous
  fallback appropriate to their iterator. No strategy is allowed to consume an input range merely to discover a size
  that would require a second traversal.

The mixed multi-argument dispatcher must also respect this source/destination pair. Normally a leading mixture of
scatter and dynamic-reserve leaves can enter the generic mixed fast path. If that classifier sees an adjacent scatter
before a marked range, however, it would premeasure and materialize the range before the output-aware dispatcher could
use the real put area. The current pre-emption rule skips that generic path only when the run contains a marked direct
leaf, the destination proves exact obuffer cursors, and that leaf is directly printable to the exact observer. The
existing checked buffered dispatcher still owns capacity refresh, short-put-area fallback, argument ordering, and the
final newline. This gate is a cost policy; the stated capability checks are the correctness proof for where it may be
applied.

The range static-stack hint and the small run-time scatter-plan scratch buffer are likewise bounded cost policies, not
semantic capabilities. They are clamped by the central byte-based stack policy, and a plan that does not fit delegates
the original descriptor sequence to the general scatter/coalescing path.

The targeted range suite includes `rgvw512`, a pure `string_view` range whose output is about 2.3 KiB. It keeps the
same element and separator protocols as the shorter cases while crossing concat's former 2-KiB inline-staging
boundary. Paired obuffer, reusable-string, portable concat, and native concat destinations distinguish a range-source
cost from a staging-boundary or destination-growth cost. The ordinary multi-argument cases now run adjacent empty
scatters and nonempty framing on both the reusable obuffer and the fast_io string sink; this catches a strategy that is
profitable only when the range happens to be the sole argument.

## Exact-resize concat contract

`precise_resize_writable_strlike<char_type, T>` is intentionally independent of `buffer_strlike`. Its CPO must first set
the destination's observable logical size to exactly `n`, then return the beginning of one contiguous array containing
at least `n` live mutable `char_type` objects. The range remains valid until a later non-const operation or destruction.
This permits a portable `std::basic_string::resize` plus writable `data()` implementation without treating spare
capacity as constructed storage or inventing put-area cursors.

An exactly measurable semantic concat run can combine that destination contract with the semantic size proof. Pack
expansion and condition selection identify the active leaves; precise leaf sizes, width padding, and the optional
newline determine one exact total. Sizing completes before the destination is resized. The implementation rejects a
total outside the pointer-difference domain, resizes once, emits directly into the live logical range, and requires the
returned cursor to equal `begin` for an empty result or `begin + exact_size` otherwise. A cursor mismatch is a contract
violation and terminates instead of publishing an incorrectly sized result.

This path is deliberately separate from bounded semantic concat. An upper bound may exceed the produced prefix and
therefore cannot be published as the destination's logical size. If exact sizing or resize throws, no destination has
escaped; if emission throws, the local result owns the partially written live characters and is destroyed during stack
unwinding. These statements establish safety and exception behavior only. Selecting the path for compact semantic
graphs is a cost heuristic, and portable strings may initialize characters during `resize` before the formatter
overwrites them.

An ordinary normalized run has a narrower exact-resize policy. When every leaf supplies the dynamic precise-reserve
protocol and the run contains at most sixteen leaves, concat measures the leaves left-to-right, caches their extents,
checks the total plus optional newline, and performs one logical resize. It then emits each leaf directly into its
adjacent slice and validates every pointer-returning writer before proceeding. The cache costs at most sixteen
`size_t` objects (128 bytes on a 64-bit ABI); larger runs retain the ordinary destination dispatcher because template
expansion and stack cost grow linearly and have no profitability evidence yet. This path never retains scatter
descriptors, so exact sizing does not imply a borrowed-source lifetime. The `print-precise-concat` benchmark suite pairs
portable `std::string` with fast_io's native put-area string at the 2/4/8/12/16 boundary for both concat and concatln.
The intermediate shapes are intentional: they separate gradual per-leaf work from nonlinear compiler code-generation
changes which would be invisible in a matrix containing only powers-of-two endpoints. Every backend specialization,
including concat, owns its complete timed loop behind a noinline boundary; GNU-compatible compilers additionally align
that boundary to 64 bytes. The attributes are outside the loop, so they add no per-operation call. They prevent
unrelated cases in this large translation unit from changing GCC's global inlining budget or moving the hot loop
across an instruction-cache line and being mistaken for a strategy effect.

## Shared small-scatter copy lowering

Print coalescing, concat materialization, and range materialization now use one `small_scatter_copy_n` policy. A
run-time payload of at most 16 elements is copied by a compact loop; larger payloads use the general non-overlapping
copy routine. The caller must already have proved destination capacity, non-overlap, and source lifetime, so this helper
changes only code generation. The cutoff is a measured heuristic: raising it can remove more out-of-line tiny `memcpy`
calls, but can also make the compiler emit a larger length-dispatch sequence. Keeping one shared boundary prevents the
three strategy layers from acquiring inconsistent thresholds.

This lowering is independent of stream scatter policy. Full-output fallback coalescing, replacing one native scatter
call with one contiguous write, and repacking several small native descriptors have separate concepts because they save
different operations. Their thresholds, chunk capacity, and minimum descriptor saving must not be inferred from the
16-element copy cutoff.

## Post-optimization Linux measurements

The concept and cursor contracts above are covered by directed correctness tests, including empty/singleton ranges,
multi-character separators, nested pack/condition composition, adjacent and framed ranges, non-idempotent output
observers, short put areas, exact-resize exceptions, and returned-end validation. Those tests establish semantics; they
do not establish profitability.

The current and frozen binaries were built by GCC 15.2 for x86-64 Linux. Nine samples per case alternated revision
order and pinned each process to logical CPU 10 after CPU 10 and its SMT sibling 11 had both measured at least 99.67%
idle. Four of sixteen E-cores were compiling during the batch, which is within the host's opportunistic 60% policy;
the median absolute deviation of the current samples was below 3% for most rows. The table therefore reports medians
and records the remaining negative cases explicitly rather than treating the data as a universal machine constant.

| Pure formatting case | Current | Frozen baseline | Change |
|---|---:|---:|---:|
| `std::string` concat, semantic `pack9` | 31.186 ns/op | 39.025 ns/op | -20.1% |
| `std::string` concat, width 255 | 10.006 ns/op | 10.799 ns/op | -7.3% |
| `std::string` concat, width 4095 | 70.741 ns/op | 109.989 ns/op | -35.7% |
| `std::string` concat, width 4096 | 69.678 ns/op | 140.961 ns/op | -50.6% |
| `std::string` concat, width 4097 | 70.919 ns/op | 145.432 ns/op | -51.2% |
| fast_io string concat, semantic `pack9` | 11.648 ns/op | 24.809 ns/op | -53.0% |
| fast_io string concat, width 4097 | 28.336 ns/op | 162.594 ns/op | -82.6% |

These semantic results validate the direct exact-resize strategy end to end. In particular, the portable
`std::string::resize` initialization write did not erase the benefit of removing the 2-KiB staging object and the
second payload transfer. The 255/256/257 and 4095/4096/4097 groups also show that the improvement is not an allocator
boundary artifact.

The ordinary precise-reserve boundary used a stricter same-source control: one header copy disabled only the ordinary
exact-resize dispatcher branch, leaving semantic concat and every producer CPO unchanged. GCC 15.2 built both binaries
with `-O3 -march=native -std=c++20`; each case performed 2.5 million operations. Eleven samples alternated variant
order on CPU 10 after CPU 10 and sibling 11 measured 99.5% and 100% idle. Every case passed its untimed byte check. The
largest relative MAD in the portable-string rows was 2.1%, and all 8/12/16-leaf rows were at or below 1.0%.

| Ordinary pure-text case | Exact resize | Staged control | Change |
|---|---:|---:|---:|
| `concat`, 2 leaves | 6.940 ns/op | 10.969 ns/op | -36.7% |
| `concatln`, 2 leaves | 7.548 ns/op | 11.237 ns/op | -32.8% |
| `concat`, 4 leaves | 12.002 ns/op | 14.940 ns/op | -19.7% |
| `concatln`, 4 leaves | 11.921 ns/op | 15.206 ns/op | -21.6% |
| `concat`, 8 leaves | 24.110 ns/op | 26.557 ns/op | -9.2% |
| `concatln`, 8 leaves | 24.811 ns/op | 27.174 ns/op | -8.7% |
| `concat`, 12 leaves | 32.947 ns/op | 35.430 ns/op | -7.0% |
| `concatln`, 12 leaves | 33.520 ns/op | 35.432 ns/op | -5.4% |
| `concat`, 16 leaves | 41.087 ns/op | 43.135 ns/op | -4.7% |
| `concatln`, 16 leaves | 39.423 ns/op | 43.444 ns/op | -9.3% |

The paired native fast_io-string cases stayed between -2.4% and +2.3%, as expected because their true put area takes
priority over the portable exact-resize fallback. GCC emitted a larger sixteen-leaf benchmark specialization for exact
resize (10,510 bytes and 1,544 instructions for concat) than for staging (7,458 bytes and 1,384 instructions), yet exact
resize was faster. This is direct evidence against selecting the policy by text size alone: the larger body removes a
staging construction and payload transfer. The exact `concatln` specialization added only 80 bytes and eight
instructions over concat, not a line-specific cliff. The earlier apparent +28.7% `concatln16` regression disappeared
once each timed loop was isolated from the translation unit's global inlining budget. Consequently newline remains a
constant suffix rather than consuming a leaf slot, and the evidence supports the common sixteen-leaf cap for both line
modes; seventeen or more leaves still use the bounded fallback.

| Range strategy case | Current | Frozen baseline | Change |
|---|---:|---:|---:|
| true obuffer, fixed 128 elements | 119.822 ns/op | 167.834 ns/op | -28.6% |
| true obuffer, adjacent run-time-scatter 128 | 236.921 ns/op | 488.097 ns/op | -51.5% |
| true obuffer, framed run-time-scatter 128 | 303.980 ns/op | 494.297 ns/op | -38.5% |
| `std::string` concat, fixed 16 elements | 31.778 ns/op | 35.840 ns/op | -11.3% |
| `std::string` concat, fixed 128 elements | 114.194 ns/op | 161.194 ns/op | -29.2% |
| fast_io string concat, fixed 128 elements | 89.267 ns/op | 140.980 ns/op | -36.7% |
| `std::string` concat, run-time-scatter 16 | 44.965 ns/op | 38.959 ns/op | +15.4% |
| `std::string` concat, run-time-scatter 128 | 292.884 ns/op | 230.213 ns/op | +27.2% |

The fixed-range and true-put-area policies are profitable at their intended boundaries. The two portable concat
run-time-scatter regressions are not hidden by that conclusion: exact-resize concat remains a separate open strategy
gap. The reusable fast_io string is governed by the narrower deferred-commit composition described above.

Two portable attempts to close that gap were rejected after same-source measurement. The C++20 candidate materialized
one initialization-sensitive borrowed dynamic-scatter source, summed its retained descriptors, reserved the exact
default-allocator `std::string` result, and appended every descriptor. Its gate admitted no custom traits, allocator,
or ADL result, and instrumentation proved one plan-size/define traversal instead of the baseline's two source
traversals. ASan and UBSan passed, but CPU-6 medians regressed from 44.816 to 100.356 ns for rgvw16 concat (+123.9%)
and from 218.025 to 734.977 ns for rgvw128 (+237.1%); concatln regressed from 45.282 to 108.461 ns (+139.5%) and from
219.143 to 741.039 ns (+238.2%). The helper was smaller (1,734/1,987 bytes versus 1,968/3,629 for concat/concatln),
but 31 or 255 tiny `std::string::append` operations retained a size, capacity, alias, length, and copy decision each.
One fewer source traversal could not repay that destination work, so amortized append capability was not promoted into
a descriptor-wise concat cost proof.

The distinct C++23 candidate retained the same validated descriptor plan but supplied it to one exact
`std::string::resize_and_overwrite` CPO. All producer calls, cursor checks, checked summation, and allocation-capacity
proofs completed before its mutable `noexcept` callback. The callback skipped empty descriptors before pointer
arithmetic, copied only into the supplied span, and returned the prevalidated extent while the borrowed plan remained
alive. GCC inlined the callback and reduced helper size to 1,475/1,870 bytes, yet its loop still made 31 or 255 tiny
conditional copies. CPU-6 medians regressed from 46.328 to 89.137 ns for rgvw16 concat (+92.4%) and from 221.084 to
692.292 ns for rgvw128 (+213.1%); concatln regressed from 46.070 to 93.091 ns (+102.1%) and from 220.522 to 680.654 ns
(+208.7%). The current staging result stays inside its 2-KiB inline buffer for these payloads and finishes with one
allocation plus one whole-payload copy. That contiguous second pass is cheaper than replacing it with fragmented final
writes, so neither portable descriptor strategy is present in the implementation.

### C++23 exact-overwrite range `concatln`

A profitable C++23 route uses a different proof and does not retain or replay scatter descriptors. A normalized source
must provide an exact size and a pointer-returning precise writer whose complete named-lvalue call is `noexcept`. Concat
computes the checked payload extent before constructing the destination, then lets `resize_and_overwrite` allocate its
final live array and invokes that source writer directly into the callback span. The callback verifies the returned end
pointer before publishing the already-checked size and writes the canonical newline in the same final array. A sizing
or allocation failure therefore occurs outside the callback; an admitted source cannot unwind through the standard
library's nonthrowing callback boundary.

For `sized_range_view_t`, admission is intentionally stronger than a nonthrowing leaf CPO. Its proof mirrors every
operation performed by the selected writer: iterator copy/destruction, dereference and preincrement;
alias/forward normalization; destruction of an owned normalized result; and the exact reserve or scatter CPO chosen
for each element. A precise writer returning `void` is excluded because the final endpoint cannot be validated. The
destination side is available only when `__cpp_lib_string_resize_and_overwrite >= 202110L`; the built-in adapter is
limited to default-traits/default-allocator `std::basic_string`, while a third-party destination must satisfy the same
callback-specific concept. The generic admission concept is false below that feature boundary, even if unrelated ADL
hooks use the same names. The tested feature-macro-absent C++20 object is consequently byte-identical to the prior
dispatcher; a library mode which exposes the standardized feature and macro as an extension is intentionally admitted.

The profitability gate is deliberately line-only. In eleven alternating 2.5-million-operation samples on an idle
physical P-core, the final line-only binary reduced `rgvw16 concatln` from 42.723 to 32.703 ns/op (-23.45%) and
`rgvw128 concatln` from 206.202 to 185.287 ns/op (-10.14%). Enabling the same route for ordinary concat improved
`rgvw16` by 2.87% but regressed
`rgvw128` from 206.982 to 209.735 ns/op (+1.33%), so non-line output retains its byte-for-byte baseline specialization.
E-core measurements favored both modes, but they do not override the P-core benchmark policy. Thus the gate records a
measured destination/source/mode combination rather than treating a semantic noexcept proof as a cost proof.

The callback state remains the explicit 24-byte `{source pointer, payload size, total size}` aggregate. Reconstructing
one extent from the other reduced the source object to two machine words, but GCC had already inlined the callback, so
no ABI transfer was removed. The reconstruction enlarged the line specialization and made E-core `rgvw128 concatln`
about 6.7% slower. Retaining all three values is therefore supported by code-generation evidence rather than an
assumption that the smallest source aggregate is necessarily the cheapest one.

Directed regressions cover named and temporary nonempty ranges, an empty range, every supported character width,
custom traits and allocators, a throwing iterator copy, a void precise writer, destination CPOs which reject the named
operation or return non-void, and a throwing size pass which must occur before the destination callback is entered.

The latest refinement was measured in a separate fifteen-sample alternating A/B batch on logical CPU 14, with its P-
core sibling CPU 15 idle. Both binaries used GCC 15 with identical function/loop/jump alignment. “Before” is the same
tree without the original cursor-independence/stream markers and direct loop; these rows therefore isolate this
refinement rather than reusing the older frozen baseline. The later direct-print-equivalence admission proof is
compile-time-only for these string sources: rebuilding with it produced a byte-identical benchmark executable.

| Deferred-commit string-like case | Before | Marked strategy | Ratio |
|---|---:|---:|---:|
| reusable fast_io string, run-time-scatter 1 | 2.149 ns/op | 2.149 ns/op | 1.000× |
| reusable fast_io string, run-time-scatter 16 | 54.537 ns/op | 29.795 ns/op | 0.546× |
| reusable fast_io string, adjacent run-time-scatter 16 | 37.968 ns/op | 31.480 ns/op | 0.829× |
| reusable fast_io string, framed run-time-scatter 16 | 36.663 ns/op | 29.699 ns/op | 0.810× |
| reusable fast_io string, run-time-scatter 128 | 422.845 ns/op | 280.603 ns/op | 0.664× |
| reusable fast_io string, adjacent run-time-scatter 128 | 294.756 ns/op | 281.878 ns/op | 0.956× |
| reusable fast_io string, framed run-time-scatter 128 | 288.176 ns/op | 283.587 ns/op | 0.984× |
| reusable fast_io string, run-time-scatter 512 | 1697.912 ns/op | 1115.486 ns/op | 0.657× |
| unmarked reusable obuffer, adjacent run-time-scatter 128 | 314.009 ns/op | 316.638 ns/op | 1.008× |

The one-element equality verifies the sixteen-element gate. The unmarked obuffer row is intentionally neutral: an
earlier structural admission improved isolated and framed ranges but regressed the already-efficient adjacent shape by
about 16%. Consequently neither arbitrary obuffers nor `basic_io_buffer_ref` inherit the marker. This is the evidence
for keeping the optimization on the string-like concept boundary rather than inferring semantic purity from cursor
syntax. Count-one measurements remain fixed-overhead diagnostics, and the shared small-copy cutoff remains a bounded
heuristic until its primitive buffered-scatter users receive a separate opaque-length assembly experiment.

The capacity-miss continuation deliberately remains in the same noinline specialization. GCC 15 initially emitted
roughly 4.9 KiB for that specialization, so two cold-split forms were tested with identical compiler flags and a
nine-sample alternating P-core matrix. Passing the continuation state as ordinary parameters reduced the hot symbol to
about 1.2 KiB but made the fitting reusable-string 16/128/512 cases 24%/13%/17% slower. Passing one aggregate address
reduced text further but regressed those cases by 31%/54%/56%: address-taking changed descriptor placement and the hot
loop's register allocation. Assembly confirmed extra moves or stack state on every fitting iteration. Both splits were
therefore rejected. Calling two raw writes was not considered an equivalent substitute because it can change pair
status dispatch, lock scope, exact-fit behavior, alias lifetime, and the prefix visible when allocation fails.

The fitting loop deliberately retains one run-time `small_scatter_copy_n` policy for every separator and element.
Several apparently obvious fixed-length refinements were measured and rejected because `t.sep.len` is invariant across
the range but the copied element length is not. A two-unit-only branch improved `::` records, yet moved the general path
such that comma records became 41--49% slower. Adding a one-unit branch recovered part of that loss but still left
comma 10.51--15.68% slower than the original helper; relative-to-intermediate gains must not be reported as a net win.
A common `len <= 2` hot branch likewise left comma 15.27--21.67% slower.

Dispatching once before the loop and instantiating fixed one-, fixed two-, and dynamic-length loop versions removed the
per-item decision, but expanded the GCC helper from 4,934 to 14,111 bytes (+186%) and the focused executable text by
20.2%. Moving explicit one/two cases into the shared primitive shrank this particular helper to 2,075 bytes, but also
put the new decision on every element copy and regressed the established two-unit records by 6.75--26.11%. Finally,
replacing separator and element copies with the smaller memcpy-shaped primitive reduced helper text by about 15% but
made the fitting cases 68--90% slower. The retained generic loop is therefore a measured multi-separator compromise;
the type-level fixed copy remains reserved for genuine `static_scatter_t<N>` extents, where it has no run-time branch.

With that generic policy restored, a focused GCC 15 baseline/current comparison put the complete timed specialization
behind the same noinline 64-byte-aligned boundary. Seven alternating physical-P-core samples measured current
`rgvw16`/`rgvw128` 19.44%/19.39% faster than the frozen tree and adjacent/framed 128-element records 7.05%/9.15%
faster. A full-harness disassembly then found one remaining internal layout dependency: the instruction-identical
4,934-byte direct helper began at cache-line offset 32 instead of zero, placing its fitting-loop entry at byte 61 of a
64-byte line rather than byte 29. Aligning that already-noinline helper to 64 bytes added 112 bytes (0.0025%) to the
complete executable and changed no helper instruction. Seven alternating samples improved full-harness
`rgvw16`/`rgvw128` by 24.93%/20.48% and adjacent/framed records by 20.14%/19.72%, exactly restoring the focused code-
shape tier. A final same-batch frozen-versus-aligned comparison measured net improvements of 18.45%/18.98% for
`rgvw16`/`rgvw128` and 16.14%/17.56% for adjacent/framed records. The apparent regressions were therefore internal
COMDAT phase artifacts, not a missing separator branch or an ABI-copy difference. Clang 21 accepted the guarded GNU
attribute and emitted `.p2align 6`; compilers without that GNU alignment attribute retain a recognized noinline-only
branch when one is available.

The final fake-call conditional-pack control retains one small, real tradeoff. Current and frozen paths both make five
opaque calls. The unified semantic plan stores and reloads five descriptors in one loop, while the frozen shape calls
the prefix and suffix directly and loops over only the three selected inner descriptors. This costs 0.107--0.151 ns/op
(4.66--6.24%) at an empty-call boundary. Reintroducing condition-specific splitting was rejected because the same
unified flattening reduces the nine-leaf fake-call pack from 8.687 to 3.950 ns/op (-54.53%). A future direct-write-only
leafwise refinement needs independent 2/5/9/16-leaf evidence; the current sub-nanosecond conditional result is reported
as an explicit scalability tradeoff, not hidden as noise.

### Exact-two passive-scatter refinement

That independent evidence admits one deliberately narrow refinement. After semantic filtering, exactly two nonempty
`basic_io_scatter_t<Char>` leaves may be emitted as two ordered typed scalar writes when the destination explicitly
prefers direct streaming. The compile-time gate excludes byte representation, native typed or byte scatter, put areas,
mutex ownership, exact status dispatch, and every established whole-output coalescing policy. Empty descriptors retain
the generic path so its observable empty-call boundary is preserved; `println` appends the canonical static newline
through the same typed write CPO rather than substituting `char_put`.

The two-leaf ceiling is an empirical code-size boundary, not an inference from printable syntax. Across 48 isolated
GCC 15 case pairs, only the eight intended plain/adjacent-condition, print/line, fake/memory cases changed; all 40
five-, nine-, sixteen-, framed-, and non-admitted controls were byte-identical. The focused executable text shrank from
109,727 to 106,791 bytes. Representative exported symbols shrank from 243 to 138 bytes for fake print, 260 to 180 for
fake line, 376 to 269 for memory print, and 409 to 279 for memory line; the adjacent-condition variants showed the same
direction. Ninety-six functional cases and the same 96 cases under ASan/UBSan passed, including status, byte
representation, native scatter, null-empty fallback, canonical-newline pointer identity, and exception-prefix checks.

Eleven alternating five-million-operation samples on an idle physical P-core measured 74.64--74.93% lower fake-boundary
cost for print and 59.82--59.99% for line. Real memory-writing cases improved by 30.71--35.38% for print and
45.82--49.74% for line. Framed non-admitted controls remained within -0.46% to +0.57%, consistent with timing noise and
their byte-identical objects. Generalizing the route without a new leaf-count/code-growth experiment would discard the
very evidence that makes this concept combination safe and profitable.

### fmt 12.2.0 controls

The fmt control used the same compiler, affinity, output bytes, iteration count, and untimed correctness preflight.
`checked` uses the ordinary typed `fmt::format_string` API: C++20 performs the type check at compile time, while the
timed call still constructs fmt's run-time argument view and executes its regular formatting path. `compile` keeps the
`FMT_COMPILE` object in the direct overload set. `fake-call` adds the same noinline assembly-protected sink after
formatting into a reused memory buffer; `string` returns a new `std::string`; and `dev-null` calls `fmt::print` on an
unbuffered `FILE*`. The fast_io rows use, respectively, the direct-write fake sink, a reusable true put area,
`concat_std`, and a POSIX file owner. Thus each row is an end-to-end API comparison at a named layer, not a claim that
the two libraries expose identical internal stages.

| Backend and workload | fast_io | fmt compile | fmt checked |
|---|---:|---:|---:|
| fake call, `pack9` | 3.940 ns/op | 23.295 ns/op | 54.005 ns/op |
| fake call, conditional pack | 2.654 ns/op | 6.121 ns/op | 21.057 ns/op |
| reusable buffer, `pack9` | 16.034 ns/op | 14.055 ns/op | 54.569 ns/op |
| reusable buffer, conditional pack | 9.683 ns/op | 4.045 ns/op | 20.214 ns/op |
| returned `std::string`, `pack9` | 34.999 ns/op | 37.820 ns/op | 62.264 ns/op |
| returned `std::string`, width 4097 | 76.442 ns/op | 2,385.029 ns/op | 1,744.689 ns/op |
| `/dev/null`, `pack9` | 89.173 ns/op | 105.573 ns/op | 143.932 ns/op |
| `/dev/null`, width 4097 | 107.006 ns/op | 1,871.536 ns/op | 1,848.402 ns/op |

The buffer rows are an important negative control: fmt's compiled program beats the current fast_io semantic
dispatcher for these compact `pack9` and conditional-pack cases, even though fast_io wins at the fake-call and returned
string layers. Conversely, large width is dominated by fmt's run-time fill/format path in this harness, while fast_io
writes directly into the proved final region. `FMT_COMPILE` therefore changes the balance substantially for compact
format programs but does not remove the run-time concat, memory-write, or syscall stages; it is not uniformly faster
than fmt's checked mode for large returned strings.

## Build and run

From the repository root, build serially:

```sh
make -j1 -C benchmark/0021.print_concepts CXX=clang++ ITERATIONS=100000
benchmark/0021.print_concepts/composition_bench --list
benchmark/0021.print_concepts/composition_bench fake-scatter print pack9
benchmark/0021.print_concepts/composition_bench obuffer println mixed
benchmark/0021.print_concepts/composition_bench std-string concat width257
benchmark/0021.print_concepts/composition_bench fast-string concatln rgvw16
benchmark/0021.print_concepts/composition_bench std-string concat precise16
benchmark/0021.print_concepts/composition_bench std-string concatln precise12
```

For Linux measurements, sample whole-machine load and per-CPU occupancy immediately before every batch. Select one
idle physical P-core, keep its SMT sibling idle, and pin exactly one benchmark process to one logical CPU from that
core. P-core and E-core work is opportunistic: never occupy more than 60% of either class, and yield or reduce the batch
as soon as another workload begins using the selected resources. Builds and correctness/random tests must be pinned to
currently idle E-cores. Cross-class activity is permitted by the host policy, but it must be recorded; alternating
revision order and sample dispersion are required to expose package-power or memory-traffic noise. A lowest-noise
confirmation should still wait for heavy compilation to clear when practical. For example, after inspection has
selected CPU 6:

```sh
taskset -c 6 benchmark/0021.print_concepts/composition_bench filebuf-owner print pack9
```

CPU affinity is deliberately external so the same binary remains usable on other machines. On macOS, run one process
at a time to respect the exact one-thread/process constraint.

## Current-versus-baseline build

Use this same source file for both builds so only the selected header tree changes. For example, after placing two
worktrees below `/tmp`:

```sh
clang++ -O3 -march=native -std=c++20 -DNDEBUG \
  -DFAST_IO_PRINT_CONCEPT_BENCH_ITERATIONS=100000 \
  -I/tmp/fast_io-current/include benchmark/0021.print_concepts/composition_bench.cc \
  -o /tmp/print-concepts-current

clang++ -O3 -march=native -std=c++20 -DNDEBUG \
  -DFAST_IO_PRINT_CONCEPT_BASELINE \
  -DFAST_IO_PRINT_CONCEPT_BENCH_ITERATIONS=100000 \
  -I/tmp/fast_io-baseline/include benchmark/0021.print_concepts/composition_bench.cc \
  -o /tmp/print-concepts-baseline
```

`FAST_IO_PRINT_CONCEPT_BASELINE` removes only tuples that the frozen headers cannot instantiate. Such a tuple is
reported as a compile-time regression and has no fabricated timing. In particular, the baseline context printer
unconditionally requests a flush operation from the reusable `obuffer` fixture, so `context3` on that backend is
current-only; all other workload/backend combinations remain in the common source and are compared normally.

Compile both binaries with identical flags, alternate their execution order on the same freshly selected idle physical
P-core, and preserve every raw sample. A smaller iteration count is appropriate for the `mixed` and wide-output
workloads; retain the same count in both binaries.

## Optional fmt comparison

`fmt_comparison.cc` is independent of the fast_io executable and is not part of the default Make target. If fmt headers
are installed, build it with:

```sh
make -j1 -C benchmark/0021.print_concepts CXX=clang++ ITERATIONS=100000 fmt
benchmark/0021.print_concepts/fmt_comparison --list
benchmark/0021.print_concepts/fmt_comparison fake-only checked pack9
benchmark/0021.print_concepts/fmt_comparison fake-call compile pack9
benchmark/0021.print_concepts/fmt_comparison memory-buffer checked width257
benchmark/0021.print_concepts/fmt_comparison string compile cond-pack
benchmark/0021.print_concepts/fmt_comparison dev-null compile width4097
```

The `checked` mode uses fmt's ordinary typed `format`/`format_to`/`print` overloads: its format-string check is performed
at compile time while run-time argument-view construction and parsing remain inside every measured call. `compile`
retains the `FMT_COMPILE` object through the corresponding direct overloads.
`string` is the materializing concat layer, `memory-buffer` performs real writes into a reused fmt buffer, `fake-call`
adds an opaque noinline write boundary after that buffer, and `dev-null` reaches fmt's FILE output layer with libc
buffering disabled so each timed call includes the actual write boundary.
`fake-only` reports the opaque call boundary by itself. All arguments that become output are strings; the width integer
is formatting metadata only and is never converted to text. If fmt headers are absent, the guarded source still
compiles to a diagnostic executable returning 77, while the core fast_io harness is unaffected.
