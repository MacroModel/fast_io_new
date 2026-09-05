# Scan, `to`, and transcoder state-machine CPO matrix

This directory isolates one policy tuple per executable.  A macro-selected
`scan_case.cc`, `to_case.cc`, or `transcoder_case.cc` never instantiates the
unrelated rows of its matrix in the timed translation unit.  This makes runtime,
compile time, compiler memory, binary size, assembly, and `llvm-mca` observations
attributable to the selected protocol rather than to a mega-benchmark's inliner
budget or code layout.

All corpus text is generated after process startup from a decimal seed.  Every
executable validates the complete corpus with a byte/value oracle before the
clock starts.  The timed path publishes values or output extents to a compiler
barrier but performs no oracle byte walk.

## Scan: input protocol × receiver state

The scan matrix is deliberately two-dimensional:

| Input protocol | Formal input transition |
|---|---|
| `contiguous` | One terminal `[current,end)` range; cursor commit is directly observable. |
| `chunk1` | Typed `read_some_underflow_define`, at most 1 character per nonempty transition. |
| `chunk3` | Same typed protocol, at most 3 characters. |
| `chunk7` | Same typed protocol, at most 7 characters. |

| Receiver | Scan grammar/state |
|---|---|
| `int10` | Signed base-10 integer, including zero and both 64-bit endpoints. |
| `int16` | Unsigned base-16 integer through `mnp::base_get<16>`. |
| `double` | Exact quarter-valued decimal strings, including signed values. |
| `string` | `std::string` context scanner with lengths through 63 characters. |

For a chunked source, a short nonempty return is progress, never EOF.  Only an
empty return is the terminal physical-input transition.  Each successful
preflight therefore checks source exhaustion, at least one primitive call, and
exactly one public stream normalization in addition to the receiver value.  In
formal terms, each case explores

```text
(physical cursor, refill result, EOF) × (receiver context, parse code, target)
```

rather than treating scanner type or input type alone as the benchmark case.

Build one tuple:

```sh
make -j1 scan FAST_IO_ROOT=../.. INPUT=chunk3 RECEIVER=int16
/tmp/fast_io_state_machine_cpo/new/scan-chunk3-int16 12345 80
```

## `to` and `inplace_to`

`to_case.cc` provides four separately compiled public front doors:

- `text-int`: runtime text to signed integer;
- `text-double`: runtime text to exact quarter-valued double;
- `scalar-string`: signed integer to `std::string`;
- `inplace-int`: runtime text into an existing signed integer.

Across the four executables, operation-specific untimed preflights check signed
minimum/maximum, scalar rendering of the minimum, positive quarter and
negative-zero floating values, empty input, a lexically invalid first character,
overflow, and failed `inplace_to` conversions.  Failure checks require an
exception but intentionally do not claim transactional rollback of the
pre-existing target: that stronger property is not part of every scanner CPO
contract.

The current official baseline exposes decimal floating scan CPOs, so the serial
runner defaults to `OLD_SUPPORTS_FLOAT_SCAN=1` and attempts the four old
`scan`/`double` rows and old `to`/`text-double` row. For a historical include
snapshot whose missing capability has been verified, explicitly set
`OLD_SUPPORTS_FLOAT_SCAN=0` to record `SKIP,no-floating-scan-cpo`. Do not infer
capability from the compiler version or silently classify a failed build as a
missing interface.

```sh
make -j1 to FAST_IO_ROOT=../.. TO=scalar-string
/tmp/fast_io_state_machine_cpo/new/to-scalar-string 12345 80
```

## Protocol-shaped `to` source packs

`to_protocol_case.cc` instantiates exactly one five-axis cell per translation
unit.  It exercises the public `to` or `inplace_to` front door; it does not call
an implementation detail in order to force a desired branch.

| Make variable | Values | Meaning |
|---|---|---|
| `TO_PROTOCOL_MODE` | `runtime`, `literal` | Seeded source objects or literals written directly at the public call site. |
| `TO_PROTOCOL_SOURCE` | `f`, `d`, `pp`, `ss`, `mixed-proof`, `literal` | Fixed reserve, dynamic reserve, precise-preferred, stable scatter, the proven heterogeneous rotation, or literal-only mode. |
| `TO_PROTOCOL_PACK` | `1`, `2`, `8`, `32` | Number of independently normalized source arguments. |
| `TO_PROTOCOL_FRONTDOOR` | `to`, `inplace` | Construct a target or update an existing target. |
| `TO_PROTOCOL_TARGET` | `builtin`, `context` | Built-in contiguous `uint64_t` scanner or the benchmark's context-only decimal state machine. |

The runtime corpus contains 1, 2, 3, 7, 8, 15, 17, and 18 digit
spellings.  Pack 32 retains 32 public arguments but uses empty suffix
fragments, keeping the complete spelling inside `uint64_t`.  An independent
untimed byte walk computes the expected value, FNV digest, length, and number
of nonempty fragments without invoking a print or scan CPO.

The context target returns `partial` for every context invocation and publishes
its state only from the terminal EOF CPO.  Its authoritative oracle is the
semantic value, byte digest, byte count, and exactly one EOF publication.
Source-to-context call counts are diagnostic only: the public conversion may
legally elide an empty source, coalesce adjacent sources, or split one source
across several context invocations.  The raw failure diagnostic retains total,
empty, and nonempty counts, but neither pass/fail nor the timed checksum treats
one implementation schedule as the protocol.  The proof-rich source types
deliberately have no counters: adding observable mutation to their size or
scatter queries would invalidate the eager-materialization, cached-size, or
stable-scatter premises which the cell is intended to test.  Their source-CPO
reachability is therefore checked with compile-time protocol assertions.

`pp` is an intentional negative dispatch control for ordinary runtime
conversion.  It publishes a valid precise protocol and its preference proofs,
but the public runtime `to` graph still uses its conservative dynamic-reserve
spelling; the cell must not be described as a forced precise branch.  The
`mixed-proof` rotation is fixed reserve, bounded dynamic, precise-preferred,
stable scatter, and alias, repeated by argument index.

Literal mode is kept separate and accepts only the built-in contiguous target.
It is labelled `literal-constant-call-lower-bound` in the executable result:
supported compilers may replace the complete public call with its constant
value.  Packs 1/2/8 and the 32-argument compile-shape stress are therefore most
useful for compile time, compiler RSS, assembly, and object/link size.  Their
runtime measurements are a constant-replacement/call lower bound, never a
runtime fallback control and never evidence that a runtime source pack took the
same path.  A literal/context cell is omitted because the public compiler-
constant fast path proves a contiguous scanner; this benchmark does not invent
an equivalent context protocol.

Build and run one cell:

```sh
make -j1 to-protocol FAST_IO_ROOT=../.. \
  TO_PROTOCOL_MODE=runtime TO_PROTOCOL_SOURCE=mixed-proof \
  TO_PROTOCOL_PACK=8 TO_PROTOCOL_FRONTDOOR=inplace \
  TO_PROTOCOL_TARGET=context
/tmp/fast_io_state_machine_cpo/new/to-protocol-runtime-mixed-proof-p8-inplace-context 12345 80
```

The protocol executable accepts only 20--80 ms and also arms an independent
800 ms process deadline.  Its raw CSV row includes the mode, source, pack,
front door, target, measured checksum, and schedule-independent untimed
validation digest.

For compile time, peak compiler RSS, object size, and linked size, build only
the selected cell and force exactly one recompilation.  On macOS use
`/usr/bin/time -lp`; on Linux use `/usr/bin/time -v`.  `to-protocol-object-path`
and `to-protocol-path` print the two isolated artifacts, so `stat`, `size`, and
Clang `-ftime-trace` data cannot accidentally aggregate several matrix cells.

```sh
/usr/bin/time -lp make -B -j1 to-protocol-object \
  TO_PROTOCOL_MODE=runtime TO_PROTOCOL_SOURCE=ss TO_PROTOCOL_PACK=32 \
  TO_PROTOCOL_FRONTDOOR=inplace TO_PROTOCOL_TARGET=context
make -s to-protocol-object-path \
  TO_PROTOCOL_MODE=runtime TO_PROTOCOL_SOURCE=ss TO_PROTOCOL_PACK=32 \
  TO_PROTOCOL_FRONTDOOR=inplace TO_PROTOCOL_TARGET=context
```

## Large-payload `to` scratch and early-stop cells

`to_large_payload_case.cc` complements the eighteen-digit protocol matrix with
runtime fragments up to 4097 bytes. Each executable owns exactly one protocol
tuple; no extra numeric scanner or unrelated operation is instantiated in its
timed translation unit. The public entry is `to<collector>` or
`inplace_to(collector, ...)`, and the receiver is a context-only byte collector.
The collector copies its accepted prefix into a bounded inline array, so the
measured operation includes destination materialization as well as formatter
scratch. The scatter control retains the same receiver and bytes while removing
formatter scratch; this is not a standalone measurement of an internal ensure
helper.

| Make variable | Values |
|---|---|
| `TO_PAYLOAD_SOURCE` | `dynamic`, `hinted-256`, `scatter` |
| `TO_PAYLOAD_PROFILE` | `reuse`, `growth`, `stop-interior`, `stop-boundary` |
| `TO_PAYLOAD_PACK` | `4`, `8` |
| `TO_PAYLOAD_FRONTDOOR` | `to`, `inplace` |
| `TO_PAYLOAD_BASELINE` | `new`, `official-old` |

The dynamic source has an exact runtime reserve-size CPO. `hinted-256` adds
only `print_reserve_static_stack_size == 256`; this is a scratch preference,
not a maximum source size. Neither type grants speculative materialization or
requires an observable query count. The old controller may legally query the
whole pack before scanning, while the current controller can defer suffix
queries. The benchmark compares those costs without treating either legal
schedule as the byte-level result.

Let `L` be the runtime leaf-size argument, admitted in `[32,4097]`:

| Profile | Per-source size schedule | Semantic result | Baseline status |
|---|---|---|---|
| `reuse` | `L,L-1,L,L-1,...` | Entire concatenated interval, one EOF publication | Old/new common |
| `growth` | `8,16,L,16,L,16,...` | Entire concatenated interval, one EOF publication | Old/new common |
| `stop-interior` | `32,L,L,L,...`; delimiter is byte 8 | First 8 bytes, no EOF publication | Old/new common |
| `stop-boundary` | `8,L,L,L,...`; delimiter is byte 8 | First 8 bytes, no EOF publication | New-only |

The names `growth` and `reuse` describe an in-operation source schedule, not a
mandated allocator policy or reuse across public calls. Every timed call starts
with a fresh target and a fresh library conversion lifetime. In particular,
small hinted fragments may use stack scratch even after a preceding large
fragment used the heap. Requests around 255/256/257 are negative/transition
controls; 2047/2048/2049 and 4095/4096/4097 expose larger staging and repeated
capacity-hit costs. P4 at `L=4097` reaches 16386 input bytes in `reuse`; P8
reaches 32772 bytes, with no decimal receiver-width restriction.

The current official old controller tests whether the returned iterator differs
from the fragment end before honoring `ok`. Thus `stop-interior` is comparable,
but success exactly at the fragment boundary is not equivalent. Both the
Makefile and the source reject `official-old` plus `stop-boundary` explicitly;
there is no emulation or misleading old timing for that cell. This capability
label must be revisited if the selected old snapshot fixes that behavior.

All eight corpus records are generated from the runtime seed in owned storage
before validation. An independent oracle walks that original storage directly,
finds the delimiter from bytes rather than source boundaries, and checks every
accepted output byte plus the exact terminal-publication count. Its digest is
computed outside timing. The timed kernel exposes its complete committed output
to an opaque memory barrier and does no hash traversal. Collector copies, if
NRVO is not performed, transport only the initialized prefix; correctness does
not depend on copying indeterminate array tails being optimized away. Inspect
`fast_io_to_large_payload_kernel` when attributing a result to scratch rather
than return-object transport.

Build a single current cell serially, from this directory, then run its bounded
profile:

```sh
make -B -j1 to-payload FAST_IO_ROOT=../.. TAG=new \
  BUILD_DIR=/tmp/fast_io_to_payload \
  TO_PAYLOAD_SOURCE=dynamic TO_PAYLOAD_PROFILE=growth \
  TO_PAYLOAD_PACK=8 TO_PAYLOAD_FRONTDOOR=inplace
/tmp/fast_io_to_payload/new/to-payload-new-dynamic-growth-p8-inplace 4097 12345 40
```

For its old pair, change only the include root, tag, and declared baseline:

```sh
make -B -j1 to-payload FAST_IO_ROOT=../../../fast_io TAG=old \
  BUILD_DIR=/tmp/fast_io_to_payload TO_PAYLOAD_BASELINE=official-old \
  TO_PAYLOAD_SOURCE=dynamic TO_PAYLOAD_PROFILE=growth \
  TO_PAYLOAD_PACK=8 TO_PAYLOAD_FRONTDOOR=inplace
```

Finish both builds before timing and run `old,new,new,old` with identical
`LEAF_BYTES SEED TARGET_MS` arguments. On SSH Linux, prefix each timed executable
with `taskset -c 14` after verifying that P-core and its sibling are idle. On M4,
use the configured Clang with `--sysroot=$SYSROOT -march=native -fuse-ld=lld`,
keep `BUILD_DIR` below `/tmp`, and do not overlap any compilation or timing job.
Executables accept only 20--80 ms samples and independently arm an 800 ms
process deadline. ASan, LSan, and UBSan runs are separate correctness evidence,
not performance samples.

`to-payload-object` builds one isolated object; `to-payload-path` and
`to-payload-object-path` print the exact artifacts for disassembly or size
collection. Each command accepts the same tuple variables. The raw CSV schema
is:

```text
operation,source,profile,pack,frontdoor,baseline,leaf_bytes,total_input_bytes,accepted_bytes,seed,iterations,seconds,ns_per_call,checksum,validation_digest
```

This fixture is intentionally not folded into `run_cases.sh`: common and
new-only profiles need separate schedules. A finite first regression uses
dynamic/hinted sources, reuse/growth/interior-stop profiles, both packs and
front doors; add matching scatter controls for P8, and run boundary-stop only
for new. Keep each tuple in its own executable and record compiler commands,
object/link sizes, and the exact include snapshot beside the runtime CSV.

## New-only streaming transcoder path

The adapter case uses current public APIs from
`fast_io_core_impl/operations/transcodeimpl/`:

```text
basic_ibuffer_view<char>
  -> make_itranscoder(crlf_to_lf)
  -> operations::transmit_until_eof
  -> make_otranscoder(lf_to_crlf)
  -> basic_obuffer_view<char>
```

The staged control first materializes the complete LF message, crosses an opaque
compiler barrier, and then materializes the CRLF result.  Both paths consume the
same canonical runtime corpus and are compared byte-for-byte outside timing.
The streaming path explicitly calls `output_stream_finish`: an lvalue adapter is
not automatically committed by the public output guard, and destruction cancels
unfinished work.  `transmit_until_eof` naturally reaches input engine finish;
the subsequent `input_stream_drain_and_finish` is an idempotent assertion that
no suffix remains unvalidated.

The EOL engines are not encryption engines.  Current input adapters publish in
`streaming_unverified` mode, so this case makes no authenticated-before-
publication claim.  A future AEAD matrix must use an adapter with a real
whole-message retention policy before it can treat a successful tag check as a
publication boundary.

```sh
make -j1 transcoder FAST_IO_ROOT=../.. TRANSCODER=adapter
make -j1 transcoder FAST_IO_ROOT=../.. TRANSCODER=staged
/tmp/fast_io_state_machine_cpo/new/transcoder-adapter 12345 80
/tmp/fast_io_state_machine_cpo/new/transcoder-staged 12345 80
```

## Direction-isolated transcoder matrix

`transcoder_direction_case.cc` separates engine dispatch, one-sided adapters,
the complete transmit composition, and a stack-only staged control.  One
translation unit instantiates exactly one row:

| `TRANSCODER_DIRECTION_MODE` | Measured boundary | Old-tree status |
|---|---|---|
| `engine-decode-member` | `crlf_to_lf::process`/`finish` member loop | Comparable direct-member engine row. |
| `engine-decode-cpo` | Native bounded `transcode_process`/`transcode_finish` CPO loop | New-only; old has no equivalent native CPO. |
| `engine-encode-member` | `lf_to_crlf::process`/`finish` member loop | Comparable direct-member engine row. |
| `engine-encode-cpo` | Native bounded process/finish CPO loop | New-only; old has no equivalent native CPO. |
| `input-adapter` | Chunked physical CRLF input through `make_itranscoder` | New-only adapter stack. |
| `output-adapter` | Logical chunks through `make_otranscoder` to a chunked sink | New-only adapter stack. |
| `full-transmit` | Input adapter, generic transmit, output adapter, and both terminal transitions | New-only adapter stack. |
| `staged-control` | Complete stack-resident decode, opaque lifetime boundary, complete encode | New-only control, not an old implementation. |

`TRANSCODER_DIRECTION_CHUNK` accepts `1`, `2`, `3`, `63`, `64`, `65`, or
`bulk`.  Finite engine rows expose at most that many source units.  Decode
supplies at least the same destination capacity; encode supplies the formal
two-units-per-source expansion bound.  Thus even the one-unit old/new engine
pair satisfies the old engine's capacity premise instead of conflating an
invalid call shape with dispatch cost.  Adapter rows apply the same limit to
the physical primitive and public read/write slices.  The generated payloads
cover 0/1/2/3, 62/63/64/65/66, 127/128/129, 255/256, and 383 logical units and
force selected CR/LF pairs across finite physical boundaries.

Every adapter is constructed and destroyed inside one timed invocation.
`default-cold` therefore means a fresh logical adapter allocation lifecycle per
call, not a promise that the process allocator returns untouched physical
pages.  `buffer64-cold` adds exactly `FAST_IO_BUFFER_SIZE=64`; the executable
rejects inherited or mislabeled buffer macros.  Engine and staged rows retain
the policy in their result label but mark it allocation-inert.  This makes the
64-byte sensitivity useful without pretending it changes a direct engine.  In
`full-transmit` it applies to both adapter buffers and transmit's temporary
transfer buffer, which is called out separately in the emitted allocation
label.

Build one current row:

```sh
make -j1 transcoder-direction FAST_IO_ROOT=../.. \
  TRANSCODER_DIRECTION_MODE=full-transmit \
  TRANSCODER_DIRECTION_CHUNK=65 \
  TRANSCODER_DIRECTION_BUFFER=buffer64-cold
$(make -s transcoder-direction-path \
  TRANSCODER_DIRECTION_MODE=full-transmit \
  TRANSCODER_DIRECTION_CHUNK=65 \
  TRANSCODER_DIRECTION_BUFFER=buffer64-cold) 12345 80
```

Only the two direct-member engine modes accept the official baseline.  The
compatibility path includes the old experimental EOL engine explicitly and
does not emulate current CPO or adapter semantics:

```sh
make -j1 transcoder-direction FAST_IO_ROOT=../../../fast_io \
  TRANSCODER_DIRECTION_BASELINE=official-old-engine \
  TRANSCODER_DIRECTION_MODE=engine-decode-member \
  TRANSCODER_DIRECTION_CHUNK=64 CXXFLAGS='-O3 -march=native -std=c++23 -DNDEBUG'
```

Preflight validates every output byte, semantic length, and logical-unit count
before calibration.  The timed checksum observes terminal success and extents
without adding a byte hash to the measured path.  Each executable accepts only
20--80 ms and arms an independent process-local 800 ms deadline before corpus
construction.  This matrix is intentionally not part of `run_cases.sh` yet;
old/new member pairs and new-only adapter rows require different schedules and
must not be collapsed into a misleading common ABBA label.

## `to` decay ABI probe

`to_decay_abi_probe.cc` is a compile-only assembly probe for the ownership
boundary shared by `to` and `inplace_to`.  Its P8 and P32 records pass prvalue
scatter descriptors into a context scanner which deliberately remains partial
until EOF.  Consequently every normalized source owner must remain live for the
complete operation, while the generated state-machine loops reveal whether a
deeper helper copied a suffix or merely borrowed that stable pack.  The probe
has no `main`; compile it with `-S` or `-c` and inspect the four unmangled entry
symbols.  Cross-target compilation must use the matching C++ headers from the
selected sysroot, for example:

```sh
clang++ --target=aarch64-linux-gnu --sysroot="$AARCH64_SYSROOT" \
  -nostdinc++ -isystem "$AARCH64_SYSROOT/include/c++/16.0.0" \
  -isystem "$AARCH64_SYSROOT/include/c++/16.0.0/aarch64-linux-gnu" \
  -I../../include -std=c++20 -O3 -S to_decay_abi_probe.cc -o /tmp/to-aarch64.s
```

The outer decay entry is intentionally still by value: it owns every prvalue
and preserves the platform ABI's ordinary aggregate classification.  References
are valid only after that boundary and only for the synchronous implementation
call.  Thus an outlined P32 owner may have a real ABI frame, whereas its inner
scan loop must contain scanner work rather than a recursive series of proxy
copy constructors.

## Serial old/new runner

`run_cases.sh` builds every available original scan and `to` tuple plus a narrow
protocol selection against the current tree and the official `../fast_io` tree,
including old floating rows by default. Only an explicit, capability-verified
historical override emits floating SKIPs. Every old/new cell executes a true `old,new,new,old` ABBA
schedule.  The transcoder adapter and its staged control are new-only, so their
explicit variants use the analogous `adapter,staged,staged,adapter` schedule
without pretending that staged is an old-tree result.  Compilation is forcibly
rebuilt with `make -B -j1`, finishes before timing starts, and removes each exact
selected output before rebuilding so a failed compile cannot run a stale binary.

The runner emits one fixed, fully quoted CSV schema:

```text
case,variant,order,repeat,seed,target,status,compiler,flags,root,raw_result
```

`order` is 1--4 within one ABBA cell and `repeat` is 1 or 2 within the
variant.  `target` records the requested measured duration.  `status` is `ok`,
`skip`, `build-failed`, `timeout`, or `exit-N`; commas, quotes, and multiline
diagnostics in every field are CSV-escaped.  A timeout or nonzero executable is
recorded without truncating later cells, and the runner exits nonzero only after
all scheduled rows have been emitted.  Compiler identity, exact flags, and the
canonical selected root travel with every row.

The default protocol selection covers every required source family, both front
doors, both target kinds, and packs 1/2/8/32 without pretending that their full
Cartesian product is equally informative:

```text
runtime:f:1:to:builtin
runtime:d:2:inplace:context
runtime:pp:8:to:builtin
runtime:ss:32:inplace:context
runtime:mixed-proof:8:inplace:builtin
literal:literal:1:to:builtin
literal:literal:2:inplace:builtin
literal:literal:8:to:builtin
literal:literal:32:inplace:builtin
```

Override `TO_PROTOCOL_CELLS` with whitespace-separated
`mode:source:pack:frontdoor:target` entries to expand or reduce that list.  Each
entry remains a separate old/new executable pair.

On SSH Linux, select the verified idle P-core 14 and leave sibling 15 idle:

```sh
CPU=14 CXX=g++ TARGET_MS=80 ./run_cases.sh > /tmp/state-machine.csv
```

Subsets require no source edits:

```sh
CPU=14 SCAN_INPUTS='contiguous chunk1' SCAN_RECEIVERS='int10 double' \
  TO_OPERATIONS='text-int inplace-int' \
  TO_PROTOCOL_CELLS='runtime:f:1:to:builtin' ./run_cases.sh
```

On Apple M4, artifacts must live under `/tmp`, the compiler must use the
configured SDK sysroot and native architecture, and no other compile or
benchmark task may run concurrently:

```sh
BUILD_DIR=/tmp/fast_io_state_machine_cpo.m4 \
CXX=/Users/liyinan/Documents/MacroModel/tool-chain/tools/aarch64-apple-darwin-llvm/llvm/bin/clang++ \
CXXFLAGS='--sysroot=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk -march=native -fuse-ld=lld -O3 -std=c++20 -DNDEBUG' \
./run_cases.sh > /tmp/state-machine-m4.csv
```

On Darwin the runner rejects every compiler other than that exact configured
Clang, every artifact path outside literal `/tmp/`, and flags missing the SDK
sysroot, `-march=native`, or `-fuse-ld=lld`.  It clears inherited make jobserver
settings and invokes each build with `-j1`; no other local compile or benchmark
task may run concurrently.

The runner accepts only 20--80 ms and defaults to 80 ms.  The approximately 1 ms
pilots leave substantial margin under the default 0.8 second outer process
deadline.  Linux uses `timeout`; Darwin arms `ITIMER_REAL` through
`Time::HiRes::ualarm` and then replaces the timer process with the benchmark via
`exec`, so no concurrent watchdog violates the M4 one-task rule.  Override
`RUN_TIMEOUT_SECONDS` only when startup instrumentation itself requires a
different bound.  Sanitizers are separate correctness passes, never a source of
performance numbers and never combined into one compiler invocation:

```sh
CXXFLAGS='-O1 -g -fno-omit-frame-pointer -fsanitize=address -std=c++20' \
CPU=14 TARGET_MS=20 ./run_cases.sh
CXXFLAGS='-O1 -g -fno-omit-frame-pointer -fsanitize=undefined -std=c++20' \
CPU=14 TARGET_MS=20 ./run_cases.sh
CXXFLAGS='-O1 -g -fno-omit-frame-pointer -fsanitize=leak -std=c++20' \
CPU=14 TARGET_MS=20 ./run_cases.sh
```

Use the same seed and compiler flags for old/new pairs.  Record compiler version,
CPU affinity/topology, and system load with the result; discard any process whose
reported elapsed time violates the configured bound.
