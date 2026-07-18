#!/usr/bin/env python3
"""Run reproducible, single-core concept-strategy benchmark samples.

The benchmark executables perform their own untimed correctness preflight.  This driver contributes only process
isolation, revision-order alternation, and machine-readable sample capture.  CPU selection deliberately remains an
argument: the caller must sample machine occupancy immediately before a run and choose one idle physical core under
the host's resource policy.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import subprocess
import sys
from collections.abc import Iterable


COMMON_WORKLOADS = (
    "leaf",
    "pack9",
    "cond-pack",
    "width255",
    "width256",
    "width257",
    "width-middle257",
    "width-right257",
    "width-internal257",
    "width4095",
    "width4096",
    "width4097",
    "rgvw1",
    "rgvw16",
    "rgvw128",
    "rgvw-fixed1",
    "rgvw-fixed16",
    "rgvw-fixed128",
    "mixed",
    "dynamic9",
    "reserve-scatter9",
    "context3",
    "staged9",
)

REPRESENTATIVE_WORKLOADS = (
    "leaf",
    "pack9",
    "cond-pack",
    "width257",
    "width4097",
    "rgvw128",
    "rgvw-fixed128",
    "mixed",
    "dynamic9",
    "reserve-scatter9",
    "context3",
    "staged9",
)

FMT_WORKLOADS = (
    "leaf",
    "pack9",
    "cond-pack",
    "width255",
    "width256",
    "width257",
    "width-middle257",
    "width-right257",
    "width4095",
    "width4096",
    "width4097",
)

SCAN_COMMON_CASES = (
    "barrier",
    "precise",
    "pack9",
    "pack64",
    "pack256",
    "fold64",
    "fold256",
    "semantic64",
    "semantic256",
    "context-pack",
    "status",
    "locked-status",
    "status-pack",
    "locked-status-pack",
)

SCAN_CURRENT_ONLY_CASES = (
    "precise-refill",
    "unmarked64",
    "unmarked256",
    "terminal-hybrid",
    "refill-context",
    "refill-hybrid",
    "mixed",
    "mixed-prefix64-terminal",
    "alias",
)

# These boundary cases were added with the three-state proxy-transport policy and therefore cannot be run by the
# frozen historical executable. Keeping them separate permits a same-source marker/control comparison without
# weakening `scan-common`, whose case vocabulary must remain valid for the historical baseline.
SCAN_PROXY_TRANSPORT_CASES = (
    "pack9",
    "pack16",
    "pack17",
    "pack32",
    "pack33",
    "pack64",
    "pack256",
)


def print_core_cases() -> list[tuple[str, ...]]:
    cases: list[tuple[str, ...]] = [("fake-only", "print", "leaf")]
    for backend in ("fake-write", "fake-scatter"):
        cases.extend((backend, "print", workload) for workload in REPRESENTATIVE_WORKLOADS)
    cases.extend(
        ("obuffer", "print", workload)
        for workload in COMMON_WORKLOADS
        if workload != "context3"
    )
    for backend in ("ostring-std", "ostring-fast"):
        cases.extend((backend, "print", workload) for workload in REPRESENTATIVE_WORKLOADS)
    for backend in ("std-string", "fast-string"):
        cases.extend((backend, "concat", workload) for workload in COMMON_WORKLOADS)
    # These ordinary multi-argument records intentionally run only on a reusable true put area. Their paired plain
    # rgvw workloads have identical sources; adjacent even has identical output bytes, isolating admission overhead.
    for workload in (
        "rgvw16-adjacent",
        "rgvw128-adjacent",
        "rgvw16-framed",
        "rgvw128-framed",
    ):
        cases.append(("obuffer", "print", workload))
    return cases


def print_io_cases() -> list[tuple[str, ...]]:
    cases: list[tuple[str, ...]] = []
    for backend in (
        "dev-null-owner",
        "dev-null-observer",
        "c-unlocked-owner",
        "c-unlocked-observer",
        "obuf-owner",
        "obuf-ref",
    ):
        cases.extend((backend, "print", workload) for workload in REPRESENTATIVE_WORKLOADS)
    for backend in ("c-owner", "c-observer"):
        cases.extend(
            (backend, "print", workload)
            for workload in ("pack9", "reserve-scatter9", "context3")
        )
    for backend in ("filebuf-owner", "filebuf-observer"):
        cases.extend(
            (backend, "print", workload)
            for workload in ("pack9", "width257", "reserve-scatter9", "context3")
        )
    return cases


def print_observer_transport_cases() -> list[tuple[str, ...]]:
    # Both backends normalize the same 128-byte identity-sensitive observer into exactly one owned proxy. The fake
    # boundary measures transport plus opaque call cost; the put-area boundary additionally performs the real payload
    # stores. One copy/op is contractual; every excess copy is a recursive framework accident. `copies/op` makes that
    # count observable instead of inferring it from wall time alone. The suite is current-only because the frozen
    # single-control concept rejected every non-trivially-copyable output before dispatch; that is a capability change,
    # not an equivalent baseline operation that can be assigned a timing.
    workloads = (
        "leaf",
        "pack9",
        "cond-pack",
        "width257",
        "rgvw128",
        "reserve-scatter9",
    )
    return [
        (backend, "print", workload)
        for workload in workloads
        for backend in ("large-fake-observer", "large-obuffer-observer")
    ]


def print_range_strategy_cases() -> list[tuple[str, ...]]:
    cases: list[tuple[str, ...]] = [
        ("obuffer", "print", "rgvw16"),
        ("obuffer", "print", "rgvw16-adjacent"),
        ("obuffer", "print", "rgvw16-framed"),
        ("obuffer", "print", "rgvw128"),
        ("obuffer", "print", "rgvw128-adjacent"),
        ("obuffer", "print", "rgvw128-framed"),
        ("obuffer", "print", "rgvw512"),
    ]
    # Plain runtime-scatter ranges expose a different strategy boundary from fixed-reserve ranges. Include every
    # contiguous destination so the targeted suite can distinguish true put-area emission, append-only materialization,
    # and exact-resize concat; count one also catches setup costs that a long range can hide.
    for count in (1, 16, 128, 512):
        workload = f"rgvw{count}"
        if count == 1:
            cases.append(("obuffer", "print", workload))
        cases.extend(
            (backend, "print", workload)
            for backend in ("ostring-std", "ostring-fast")
        )
        cases.extend(
            (backend, "concat", workload)
            for backend in ("std-string", "fast-string")
        )
    for count in (1, 16, 128):
        workload = f"rgvw-fixed{count}"
        cases.extend(
            (backend, "print", workload)
            for backend in ("obuffer", "ostring-std", "ostring-fast")
        )
        cases.extend(
            (backend, "concat", workload)
            for backend in ("std-string", "fast-string")
        )
    return cases


def print_semantic_concat_cases() -> list[tuple[str, ...]]:
    # Keep this suite narrowly focused on exact semantic composition. The paired destinations separate the portable
    # std::string exact-resize path from fast_io's native string control without mixing in output-device protocols.
    workloads = (
        "pack9",
        "cond-pack",
        "width255",
        "width256",
        "width257",
        "width4095",
        "width4096",
        "width4097",
    )
    return [
        (backend, "concat", workload)
        for workload in workloads
        for backend in ("std-string", "fast-string")
    ]


def print_precise_concat_cases() -> list[tuple[str, ...]]:
    # This suite isolates the bounded ordinary all-precise strategy. std-string exercises portable exact resize;
    # fast-string is the native put-area control. Eight and twelve leaves resolve the profitability boundary and expose
    # a nonlinear compiler effect before the cap; concatln repeats every plan with checked line ownership.
    return [
        (backend, operation, workload)
        for workload in ("precise2", "precise4", "precise8", "precise12", "precise16")
        for operation in ("concat", "concatln")
        for backend in ("std-string", "fast-string")
    ]


def print_reserve_scatter_cases() -> list[tuple[str, ...]]:
    # Every record prints the same nine retained reserve-scatter leaves. The backends deliberately span a raw
    # write-only sink, a native scatter sink, reusable contiguous buffers, and the hosted adapter layers. Comparing
    # this suite with a build that disables only the buffered-run selector therefore measures the admission decision;
    # it does not mix that decision with a different workload or with unrelated semantic-formatting machinery.
    return [
        (backend, "print", "reserve-scatter9")
        for backend in (
            "fake-write",
            "fake-scatter",
            "obuffer",
            "ostring-std",
            "ostring-fast",
            "dev-null-owner",
            "dev-null-observer",
            "c-unlocked-owner",
            "c-unlocked-observer",
            "obuf-owner",
            "obuf-ref",
            "c-owner",
            "c-observer",
            "filebuf-owner",
            "filebuf-observer",
        )
    ]


def print_direct_range_cases() -> list[tuple[str, ...]]:
    # fake-write has no reusable put area and explicitly opts into cheap direct streaming. A paired build with that
    # one CPO removed is the clean control for the source-by-destination cost proof. The native scatter backend is
    # retained as an invariant control: its higher-priority scatter plan must be unchanged by the streaming marker.
    return [
        (backend, "print", workload)
        for workload in ("rgvw1", "rgvw16", "rgvw128", "rgvw512")
        for backend in ("fake-write", "fake-scatter")
    ]


def fmt_cases() -> list[tuple[str, ...]]:
    cases: list[tuple[str, ...]] = [("fake-only", "checked", "leaf")]
    for backend in ("fake-call", "memory-buffer", "string", "dev-null"):
        for mode in ("checked", "compile"):
            cases.extend((backend, mode, workload) for workload in FMT_WORKLOADS)
    return cases


SUITES: dict[str, Iterable[tuple[str, ...]]] = {
    "print-core": print_core_cases(),
    "print-io": print_io_cases(),
    "print-observer-transport": print_observer_transport_cases(),
    "print-range-strategy": print_range_strategy_cases(),
    "print-semantic-concat": print_semantic_concat_cases(),
    "print-precise-concat": print_precise_concat_cases(),
    "print-reserve-scatter": print_reserve_scatter_cases(),
    "print-direct-range": print_direct_range_cases(),
    "fmt": fmt_cases(),
    "scan-common": ((case,) for case in SCAN_COMMON_CASES),
    "scan-current-only": ((case,) for case in SCAN_CURRENT_ONLY_CASES),
    "scan-proxy-transport": ((case,) for case in SCAN_PROXY_TRANSPORT_CASES),
}


def parse_variant(value: str) -> tuple[str, pathlib.Path]:
    name, separator, path = value.partition("=")
    if not separator or not name or not path:
        raise argparse.ArgumentTypeError("variant must be NAME=/absolute/path")
    executable = pathlib.Path(path)
    if not executable.is_absolute():
        raise argparse.ArgumentTypeError("variant executable path must be absolute")
    return name, executable


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suite", choices=sorted(SUITES), required=True)
    parser.add_argument("--cpu", type=int, required=True)
    parser.add_argument("--samples", type=int, default=7)
    parser.add_argument("--variant", type=parse_variant, action="append", required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    arguments = parser.parse_args()

    if arguments.cpu < 0 or arguments.samples < 1:
        parser.error("cpu must be nonnegative and samples must be positive")
    variants: list[tuple[str, pathlib.Path]] = arguments.variant
    missing = [str(path) for _, path in variants if not path.is_file()]
    if missing:
        parser.error("missing executable(s): " + ", ".join(missing))

    nanoseconds_patterns = (
        re.compile(r"(?:^|\s)ns/op=([0-9]+(?:\.[0-9]+)?)"),
        re.compile(r"(?:^|\s)([0-9]+(?:\.[0-9]+)?)\s+ns/op(?:\s|$)"),
    )
    records: list[dict[str, object]] = []
    failures: list[dict[str, object]] = []
    cases = list(SUITES[arguments.suite])

    for case_index, case in enumerate(cases):
        for sample in range(arguments.samples):
            # Alternating both sample parity and case parity prevents one revision from always receiving the first,
            # colder process slot while retaining a deterministic order for exact reproduction.
            ordered = variants if (sample + case_index) % 2 == 0 else list(reversed(variants))
            for variant_name, executable in ordered:
                command = ["taskset", "-c", str(arguments.cpu), str(executable), *case]
                completed = subprocess.run(command, text=True, capture_output=True, check=False)
                output = completed.stdout.strip()
                match = next(
                    (candidate for pattern in nanoseconds_patterns if (candidate := pattern.search(output))),
                    None,
                )
                record: dict[str, object] = {
                    "suite": arguments.suite,
                    "cpu": arguments.cpu,
                    "case": list(case),
                    "variant": variant_name,
                    "sample": sample,
                    "returncode": completed.returncode,
                    "ns_per_operation": float(match.group(1)) if match else None,
                    "output": output,
                }
                records.append(record)
                if completed.returncode != 0 or match is None:
                    record["stderr_tail"] = completed.stderr.strip()[-1000:]
                    failures.append(record)

    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text(json.dumps(records, indent=2) + "\n", encoding="utf-8")
    print(f"suite={arguments.suite} cases={len(cases)} records={len(records)} failures={len(failures)}")
    if failures:
        for failure in failures[:10]:
            print(
                "failure variant={variant} case={case} returncode={returncode}".format(**failure),
                file=sys.stderr,
            )
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
