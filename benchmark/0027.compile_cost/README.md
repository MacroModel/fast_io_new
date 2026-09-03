# fast_io isolated compile-cost matrix

This Linux runner compares historical and current `fast_io` include trees with
one isolated translation unit per case and two independent compiler processes:

1. a syntax/template-front-end pass using `-fsyntax-only`;
2. an optimized object pass using `-O2 -c` by default.

There is no link step.  The fixed compiler order is GCC 11 through 16 followed
by Clang 17 through 23.  Cases and repeats are nested below each compiler.  The
revisions remain adjacent within every repeat: odd repeats use the declared
order, while even repeats reverse it.  Thus `--repeat 2` produces
`old,new,new,old` for each compiler/case and balances first/second order bias.

The runner refuses to execute a compiler on non-Linux hosts.  `--list-cases`
and `--dry-run` remain available on macOS and invoke no compiler, preserving the
separate M4 testing contract.

## Basic Linux run

```console
python3 benchmark/0027.compile_cost/run_compile_cost.py \
  --include-root old=/path/to/fast_io \
  --include-root new=/path/to/fast_io_new \
  --cpu 16 \
  --repeat 2 \
  --output /tmp/fast_io-compile-cost.csv
```

An include-root may name either a repository root or its `include` directory.
The defaults are `old=../fast_io` and `new=` the repository containing this
runner.  GNU `/usr/bin/time` wraps `taskset -c 16` and each compiler process.
The default timeout is 0.8 seconds per compiler or object-inspection process;
compiler and GNU-time version probes also have a fixed 0.8-second timeout.
`--timeout-seconds` can relax the case/tool limit for an intentionally heavy
probe, but changes the experiment's short-sample contract.

Missing canonical compiler names are `SKIP`, including an unavailable
`clang++-23`.  An explicitly overridden compiler that is missing or reports the
wrong family/major is `FAIL`:

```console
python3 benchmark/0027.compile_cost/run_compile_cost.py \
  --compiler clang23=/opt/llvm-23/bin/clang++ \
  --include-root old=/srv/fast_io \
  --include-root new=/srv/fast_io_new \
  --cpu 16 \
  --output /tmp/compile-cost.csv
```

`--only-compiler gcc16 --only-compiler clang22` retains canonical order while
running a smaller preflight.  `--case print_pack_32` similarly restricts cases.
Use `--artifact-root PATH` to retain `case.cc`, `case.o`, separate syntax/object
GNU-time files, and separate diagnostic logs.  Without it, the unique case
directories are removed after capture.  An existing CSV is never replaced
unless `--overwrite` is explicit.

## Cases and extension slots

Built-in common-interface cases are:

- `print_pack_1`, `print_pack_8`, and `print_pack_32`;
- `concat_pack_1`, `concat_pack_8`, and `concat_pack_32`;
- `scan_scalar`, which enters through public `fast_io::io::scan<true>` and
  instantiates the input/target state machine;
- `transmit_all`, which instantiates an exact-count input-to-output transfer.

The `to_extension`, `scan_extension`, `transmit_extension`, and
`transcoder_extension` rows are stable extension interfaces for more elaborate
CPO combinations. Each source must be a complete C++ translation unit. A
source selector and an entry-point selector may be common or revision-qualified:

```console
python3 benchmark/0027.compile_cost/run_compile_cost.py \
  --extension-source scan=/srv/probes/common_scan.cc \
  --extension-symbol scan=fast_io_scan_compile_oracle \
  --extension-source transmit@old=/srv/probes/old_transmit.cc \
  --extension-symbol transmit@old=fast_io_old_transmit_compile_oracle \
  --extension-source transmit@new=/srv/probes/new_transmit.cc \
  --extension-symbol transmit@new=fast_io_new_transmit_compile_oracle \
  --extension-source transcoder@new=/srv/probes/new_transcoder.cc \
  --extension-symbol transcoder@new=fast_io_transcoder_compile_oracle \
  --include-root old=/srv/fast_io \
  --include-root new=/srv/fast_io_new \
  --cpu 16 \
  --output /tmp/compile-cost-extended.csv
```

Every supplied extension source requires a matching `--extension-symbol`.  The
source must define that exact unmangled name as an externally visible text
symbol, normally a no-inline `extern "C"` function that calls the intended CPO
path and makes its result observable.  The optimized object is accepted only
when `nm` reports the exact symbol with type `T` or `W`.  This is an
**entry-point retention oracle**: it proves that the named wrapper was emitted
as external text, rather than internalized or replaced by data.  It does not by
itself prove that every operation inside the wrapper survived optimization.
That stronger claim requires an observable dependency checked by the source
protocol, or a separate disassembly audit.

The source is copied into a unique case directory.  Its original directory is
passed with `-iquote` to preserve local quoted includes.  No wrapper, `main`, or
implicit API adapter is injected.  Revision-specific protocols therefore stay
explicit rather than creating a false old/new compatibility claim.

## Strict result classification

`PASS` requires both timed compiler processes to return zero, both GNU-time
records to contain wall/user/system/RSS/exit fields, a nonempty object to exist,
section and symbol extraction to succeed, and the exact external-text entry
point to survive.

`SKIP` is limited to unavailable optional capability:

- a canonical compiler executable is absent and was not explicitly overridden;
- an extension source was not supplied for that revision.

`FAIL` covers every attempted or misconfigured measurement, including an
invalid include root, missing explicit compiler, compiler family/major mismatch,
missing declared extension file or entry-point symbol, unavailable measurement
tool or requested CPU, timeout, nonzero pass, missing object, malformed timing
data, unparsable object metrics, or a missing/wrong-kind entry point.
Diagnostics never become `SKIP`.  The process exits zero for only `PASS`/`SKIP`,
one if any row is `FAIL`, and two for command-line errors.

## Measurement contract

The syntax pass uses C++20, `-DNDEBUG`, and `-fsyntax-only`.  Its wall/user/system
and peak-RSS values include driver startup, preprocessing, parsing, semantic
analysis, and template instantiation.  They exclude optimization, code
generation, assembly, object inspection, and linking.  Because preprocessing
and compiler startup remain included, these fields are a reproducible
front-end proxy, not a template-only internal compiler timer.

The object pass starts a fresh compiler process over the same source and uses
C++20, `-O2 -c`, `-DNDEBUG`, and function/data sections by default.  Its timing
therefore includes a second preprocessing/parsing/instantiation traversal plus
optimization, code generation, and assembly.  It excludes linking, `size`, and
`nm`.  Do not subtract syntax time from object time as if internal phases were
additive, and do not add the two peak-RSS maxima.

`--cxxflag=-FLAG` applies to both passes.  `--syntax-flag=-FLAG` and
`--object-flag=-FLAG` are pass-specific.  Exact argv arrays and hashes are
recorded separately.  Changing flags, standard, optimization, timeout, CPU, or
include roots changes the experiment.

`object_bytes` is filesystem size.  `text_bytes` sums `.text` and `.text.*`;
`rodata_bytes` sums `.rodata` and `.rodata.*` from `size -A -d`.
`defined_symbol_count` counts records from
`nm -a --defined-only --format=posix`.  These are unlinked object metrics.

## CSV schema version 2

| Column | Meaning |
| --- | --- |
| `schema_version` | Literal `2`. |
| `run_id` | UUID shared by one invocation. |
| `sequence` | One-based actual execution order, including alternating revisions. |
| `timestamp_utc` | Row completion timestamp in ISO 8601 UTC. |
| `host` | Linux hostname. |
| `revision` | Label from `--include-root`. |
| `include_root` | Normalized `-I` directory, or invalid requested path on `FAIL`. |
| `compiler` | Canonical label such as `gcc15` or `clang22`. |
| `compiler_family`, `compiler_major` | Required compiler identity. |
| `compiler_path`, `compiler_version` | Resolved executable and first version line. |
| `case`, `case_family`, `pack_size` | Stable case identity and optional pack cardinality. |
| `repeat` | One-based repeat index. |
| `status`, `reason` | `PASS`/`FAIL`/`SKIP` and stable classification detail. |
| `syntax_wall_seconds`, `syntax_user_seconds`, `syntax_system_seconds` | GNU time `%e`, `%U`, `%S` for only the `-fsyntax-only` process. |
| `syntax_peak_rss_kib`, `syntax_exit_status` | GNU time `%M`, `%x` for the syntax process. |
| `object_wall_seconds`, `object_user_seconds`, `object_system_seconds` | GNU time `%e`, `%U`, `%S` for only the optimized `-c` process. |
| `object_peak_rss_kib`, `object_exit_status` | GNU time `%M`, `%x` for the object process. |
| `object_bytes` | Filesystem size of `case.o`. |
| `text_bytes`, `rodata_bytes` | Summed text and read-only-data section families. |
| `defined_symbol_count` | Defined `nm` records, including local symbols. |
| `dce_oracle_symbol`, `dce_oracle_symbol_type` | Historical Linux field names for the required entry point and observed `T`/`W` type. |
| `source_sha256` | SHA-256 of the exact copied/generated TU. |
| `syntax_command_sha256`, `syntax_command_json` | Hash and exact argv for the syntax pass. |
| `object_command_sha256`, `object_command_json` | Hash and exact argv for the object pass. |
| `diagnostic_excerpt` | Whitespace-normalized first 2000 labeled diagnostic characters. |
| `artifact_directory` | Retained case directory, or empty without `--artifact-root`. |

Inspect without compilation:

```console
python3 benchmark/0027.compile_cost/run_compile_cost.py --list-cases
python3 benchmark/0027.compile_cost/run_compile_cost.py --dry-run \
  --include-root old=../fast_io --include-root new=. \
  --only-compiler clang23 --repeat 2
```

## Serial Darwin/M4 runner

`run_compile_cost_darwin.py` is the Darwin counterpart for the single-task M4
contract.  It imports the case renderers, extension slots, selector rules, and
exact-symbol protocol from `run_compile_cost.py`; there is no second copy of a
generated case to drift.  Compiler execution is refused unless the host reports
Darwin.  `--list-cases` and `--dry-run` are available on any host and do not
probe or invoke Clang.

Every selected case has exactly four serial samples in this fixed order:

```text
old repeat 1, new repeat 1, new repeat 2, old repeat 2
```

Thus the M4 runner always performs the balanced `old,new,new,old` comparison.
It accepts exactly one Clang driver rather than a compiler matrix.  The default
is the currently provisioned explicit driver:

```text
/Users/liyinan/Documents/MacroModel/tool-chain/tools/aarch64-apple-darwin-llvm/llvm/bin/clang++
```

Override it with `--compiler /absolute/path/to/clang++`.  The driver is resolved
and its version banner must identify Clang; a missing or non-Clang executable is
a `FAIL`, never an availability `SKIP`.  The default sysroot is
`/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk`.  Every measured compiler
argv contains both `--sysroot=PATH` and `-march=native`, and the normalized
values are also CSV columns.  `--fuse-ld=lld` is supported and retained in the
recorded argv.  Neither pass links, so this option selects driver policy without
adding linker startup to the measurement.

The two passes remain independent processes:

1. C++20 and `-fsyntax-only` for front-end/template cost;
2. a fresh C++20 `-O2 -c` process for optimization, code generation, assembly,
   and an unlinked Mach-O object.

Use `--standard c++23` for a separate C++23 matrix.  The fixed standard,
`-march=native`, sysroot, `-O2`, compile mode, and LTO boundary cannot be
overridden through extra flags.  The validator rejects every user `-O*` or
`-m*` spelling (including `-Ofast`, `-Og`, `-mcpu`, and backend forwarding),
function/data-section overrides, architecture/target selectors, `-Xclang`,
conditional-architecture and assembler forwarding, response files, Clang
configuration files, module-cache overrides, and equivalent target escape
hatches.  The default per-process timeout is 120 seconds, not the Linux
benchmark's 0.8-second runtime cap: compile-cost probes can legitimately spend
several seconds instantiating a long pack.  An intentional heavier case may use
`--timeout-seconds N`.

Compiler processes do not inherit the invoking shell environment.  The runner
constructs a closed environment containing only an isolated `HOME`, `TMPDIR`,
XDG cache/config roots under the run artifact directory, fixed C locales, and
the fixed system `PATH` `/usr/bin:/bin:/usr/sbin:/sbin`.  Consequently inherited
`CPATH`, `CPLUS_INCLUDE_PATH`, `SDKROOT`, deployment-target, Clang configuration,
`CCC_OVERRIDE_OPTIONS`, and module-cache variables cannot change one run.
`--no-default-config` disables driver configuration discovery, and each pass
receives a distinct `-fmodules-cache-path` below its case directory.  The exact
effective environment policy and its SHA-256 are written to
`environment_policy.json` and repeated in each CSV row.

Darwin `/usr/bin/time -l` wraps each compiler invocation independently and
writes a separate timing file.  The runner records wall, user, system, process
exit status, and maximum resident set size.  On Darwin, `maximum resident set
size` is reported in **bytes**, so the columns are named
`syntax_peak_rss_bytes` and `object_peak_rss_bytes`; they must not be compared
to the Linux `*_peak_rss_kib` columns without conversion.

After a successful object pass, `/usr/bin/otool -l` measures the exact Mach-O
`__text`, `__const`, and `__cstring` sections, and filesystem size supplies
`object_bytes`.  Missing read-only sections are recorded as zero; a missing
`__text` section, or a named section for which no valid `size` record was
actually consumed, is a measurement failure.  `/usr/bin/nm -g -U` must report
the exact Mach-O external spelling (`_` followed by the declared C identifier)
with text type `T` or `W`.  This exact match is the entry-point retention
oracle, adapted for Mach-O's leading underscore rather than weakened to a
substring match.  It does not establish retention of operations inside the
function; that requires a checked data dependency or disassembly.

All generated/copied sources, objects, timing records, compiler diagnostics,
`otool` output, and `nm` output live under one unique directory named
`/tmp/fast_io_compile_cost_darwin.*`.  Artifacts are retained and their case
directory is recorded in every materialized CSV row (preflight failures and
optional-extension `SKIP` rows have no case directory).  No PCH, object,
compiler process, or module cache is reused between samples.  The CSV is
created exclusively unless `--overwrite` is explicit.

Every selected extension TU is opened and frozen exactly once before its ABBA
schedule begins; a common old/new path shares the same immutable byte snapshot.
Each sample copies those frozen bytes, and the runner compares file identity,
size, modification time, and change time before and after the measurement.  A
source replacement or mutation is `FAIL`; it is never silently accepted by
rereading a different TU for a later ABBA member.

Example execution (run only when the M4 compile slot is free):

```console
python3 benchmark/0027.compile_cost/run_compile_cost_darwin.py \
  --include-root old=/path/to/fast_io \
  --include-root new=/path/to/fast_io_new \
  --compiler /absolute/path/to/clang++ \
  --sysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk \
  --case concat_pack_32 \
  --output /tmp/fast_io-compile-cost-darwin.csv
```

Extension sources use the same `SLOT[@REVISION]=PATH` and exact extern-C symbol
options documented above.  An absent extension remains `SKIP`; a declared but
missing source, missing entry-point symbol, source mutation, compile failure,
timing parse failure, timeout, Mach-O inspection failure, or wrong entry-point
type is `FAIL`.  A run exits one if any row fails and zero when every row is
`PASS` or an optional-extension `SKIP`; command-line errors exit two.

Darwin schema `darwin-2` retains the Linux audit concepts while making platform
units, object formats, and entry-point terminology explicit.  It records the
normalized include root, requested and resolved compiler paths, Clang
major/version, standard, sysroot,
architecture policy, optional linker policy, case/repeat/actual sequence,
status/reason, both independent timing groups, object and Mach-O section sizes,
the logical and Mach-O entry-point spellings/type, exact source SHA-256,
environment policy JSON/SHA-256, exact argv JSON/SHA-256 for both passes, a
bounded diagnostic excerpt, and the retained artifact directory.  Each pass
also records a comparable command hash after replacing only its ABBA-varying
case-artifact, include-root, and extension-directory paths with named
placeholders.  Equal comparable hashes prove that the remaining driver policy
is identical; source hashes remain the separate content comparison.

Inspect the Darwin plan without compilation:

```console
python3 benchmark/0027.compile_cost/run_compile_cost_darwin.py --list-cases
python3 benchmark/0027.compile_cost/run_compile_cost_darwin.py --dry-run \
  --include-root old=../fast_io \
  --include-root new=. \
  --case concat_pack_32
```
