# Short old/new numeric CPO matrix

This harness compiles one integer carrier width, radix, and argument count into
each executable so that unrelated template instantiations cannot change the
inliner budget or hot code placement. The same source is intended to be
compiled once against this tree and once against `../fast_io`; only
public/common formatting and scanning protocols are used.

Every invocation measures one operation family and accepts an explicit
iteration count.  Keep a sample between 0.1 and 0.8 seconds and always below
one second.  `reserve` is the normalized direct formatter control, `print`
targets a fixed output view, `concat-fast` constructs a fast_io string,
`concat-std` constructs a default standard string, `parse-by-scan` measures
the public contiguous pointer-range parser, `stream-scan` adds input-view
normalization and stream dispatch, and `to`/`inplace-to` exercise the
format-then-scan conversion boundary. The input corpus contains 4096
deterministic values of exactly the requested digit width. Supported radices
are 2 through 36; `to` and `inplace-to` deliberately retain the ordinary
decimal grammar. An 8-bit carrier is a character type on the measured ABIs, so
the numeric base-10 print and scan cases deliberately use explicit
`base<10>`/`base_get<10>` manipulators; the raw character CPO would measure a
different grammar. `to` and `inplace-to` reject that carrier instead of
mislabeling its character conversion as an integer result. Each format mode
performs an untimed, full-byte comparison against an independent radix
formatter over the complete corpus before its compact timed checksum is used;
scan and conversion modes likewise validate every value plus parser cursor and
status where those observations are part of the public result.
The timed integer corpus is unsigned; signed and negative spellings remain in
the adjacent fixed-view correctness test rather than being silently presented
as signed performance evidence.

`floating_scan_matrix.cc` supplies the corresponding finite binary64 control
for scalar `to`, `inplace_to`, contiguous parsing, and stream-CPO scanning.
Its `short` and `long` corpora cross sign, radix-point, exponent,
long-significand, and extreme finite-exponent paths without timing source
allocation or exceptional overflow handling.

`integer_codegen_probe.cc` exports stable C symbols for the normalized uint64
decimal reserve leaf and the public fixed-output CPO. Its inert LLVM-MCA region
comments permit the same translation unit to drive source-level smoke tests,
old/new assembly diffs, instruction and symbol-size accounting, and llvm-mca
throughput analysis without accidentally measuring `main` or benchmark-loop
bookkeeping.

`transmit_matrix.cc` isolates six transfer families: element all/some/EOF and
byte all/some/EOF (`FAST_IO_OLD_NEW_TRANSMIT_KIND=0..5`).  The input primitive
publishes at most `FAST_IO_OLD_NEW_TRANSMIT_CHUNK` bytes per partial read, so
1/3/7/23-byte cells distinguish short progress from physical EOF.  Output mode
0 is a typed/byte overflow observer; mode 1 is a fixed public obuffer view.
Thus each row is an explicit input-state-machine × output-capability product.

`FAST_IO_OLD_NEW_TRANSMIT_CAPACITY` configures the fixture's input and output
storage and defaults to 4096, preserving existing binaries and commands.
`FAST_IO_OLD_NEW_TRANSMIT_REQUEST_BOUND` records the independently audited
maximum staging request made by the selected library: 131072 bytes on ordinary
32/64-bit targets, or 4096 on targets whose `size_t` is at most 16 bits. A
compile-time check rejects a mismatched oracle premise. Partial-read call counts use
`min(TRANSMIT_CHUNK, TRANSMIT_REQUEST_BOUND)`, while exact-read counts use the
request bound directly. This distinction is observable only when the payload or
declared fixture chunk exceeds a staging window. The macro records an oracle
premise; it must be re-audited rather than guessed when testing another tree.

The available and requested counts remain runtime arguments across a noinline
public-CPO boundary, so a zero-count exact/some sample still reveals an eager
staging allocation instead of being folded away as a compile-time constant.
Passing both counts also models early EOF independently of fragmentation:
`AVAILABLE < REQUESTED` is valid for `some`, while exact transfer rejects that
invalid precondition. EOF has no count parameter, drains `AVAILABLE`, and must
perform one empty read to discover the terminal state; the oracle checks that
separately rather than applying the zero-count identity illegally.
Deterministic raw bytes receive a full untimed comparison, the complete
destination escapes through a memory barrier, and exact primitive-call counts
are asserted to detect premature EOF and duplicate publication. The combined
input/output normalization count is reported in the result but is not currently
asserted by the preflight oracle; it must not be cited as an independently
verified exactly-once normalization guarantee. Every process
arms an independent 800 ms `ITIMER_REAL` deadline, including validation, so an
unexpected state-machine loop cannot violate the sub-second experiment rule.

One serial case can be selected through the Makefile without editing source:

```sh
make -j1 transmit ROOT=../.. TRANSMIT_KIND=5 TRANSMIT_CHUNK=3 \
  TRANSMIT_OUTPUT=1
./transmit_matrix 4096 4096 10000
```

The middle argument is optional and defaults to `AVAILABLE`; for example,
`./transmit_matrix 24 4096 10000` exercises a bounded transfer that reaches
physical EOF after fragmented 23-byte plus 1-byte progress reads.

Large-payload runners may append the compile-time capacity and request bound as
an identity pair without changing the result CSV schema:

```sh
./transmit_matrix 131073 131073 1000 131074 131072
```

The process rejects a mismatched identity before validation or timing. A large
fixed-output fixture should keep at least one spare element beyond the maximum
payload. This avoids conflating the official baseline's exact-capacity
`basic_obuffer_view` behavior with the EOF/transmit boundary being tested.

On Apple M4, build serially into `/tmp` with the configured upstream Clang and
LLD:

```sh
repo=$(pwd)
old_repo=$(cd ../fast_io && pwd)
run_dir=$(mktemp -d /tmp/fast_io_numeric.XXXXXX)
cd /tmp
clang++ --sysroot="$SYSROOT" -fuse-ld=lld -march=native -O3 -std=c++20 -DNDEBUG \
  -DFAST_IO_OLD_NEW_NUMERIC_BITS=64 \
  -DFAST_IO_OLD_NEW_NUMERIC_BASE=10 -DFAST_IO_OLD_NEW_NUMERIC_PACK=1 \
  -I"$repo/include" "$repo/benchmark/0025.old_new_cpo/numeric_matrix.cc" \
  -o "$run_dir/new"
clang++ --sysroot="$SYSROOT" -fuse-ld=lld -march=native -O3 -std=c++20 -DNDEBUG \
  -DFAST_IO_OLD_NEW_NUMERIC_BITS=64 \
  -DFAST_IO_OLD_NEW_NUMERIC_BASE=10 -DFAST_IO_OLD_NEW_NUMERIC_PACK=1 \
  -I"$old_repo/include" "$repo/benchmark/0025.old_new_cpo/numeric_matrix.cc" \
  -o "$run_dir/old"
"$run_dir/old" print 9 30000000
"$run_dir/new" print 9 30000000
```

The 30-million sample is only an M4 starting point: calibrate each operation
family independently, because pack size, radix, allocation, and scan grammar
change its cost materially. Reject a sample outside the 0.1--0.8 second window
instead of reusing its iteration count mechanically.

For Linux timing, compile snapshots in unique `/tmp` directories, pin each
timed process to one idle P-core, leave its SMT sibling idle, alternate old/new
process order, and discard a batch if package throttling changes.  Sanitizer
validation is a separate `-O1 -g -fno-omit-frame-pointer
-fsanitize=address,leak,undefined` pass and supplies no performance number.
