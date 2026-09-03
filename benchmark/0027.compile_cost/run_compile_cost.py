#!/usr/bin/env python3
"""Measure isolated fast_io old/new translation-unit compilation costs.

The runner is intentionally Linux-only for execution.  Listing and dry-run
operations remain available elsewhere so that a matrix can be reviewed on the
development machine without accidentally compiling it there.
"""

from __future__ import annotations

import argparse
import csv
import dataclasses
import datetime as dt
import hashlib
import json
import os
import pathlib
import platform
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import uuid
from collections.abc import Callable, Sequence


SCRIPT_DIRECTORY = pathlib.Path(__file__).resolve().parent
REPOSITORY_ROOT = SCRIPT_DIRECTORY.parents[1]
SCHEMA_VERSION = "2"
LABEL_PATTERN = re.compile(r"[A-Za-z0-9_.-]+\Z")
EXTERNAL_SYMBOL_PATTERN = re.compile(r"[A-Za-z_][A-Za-z0-9_]*\Z")
CPU_LIST_PATTERN = re.compile(r"\d+(?:-\d+)?(?:,\d+(?:-\d+)?)*\Z")
GNU_TIME_FORMAT = "wall=%e\nuser=%U\nsystem=%S\npeak_rss_kib=%M\nexit_status=%x"
EXTENSION_SLOTS = ("to", "scan", "transmit", "transcoder")
PROBE_TIMEOUT_SECONDS = 0.8


CSV_FIELDS = (
    "schema_version",
    "run_id",
    "sequence",
    "timestamp_utc",
    "host",
    "revision",
    "include_root",
    "compiler",
    "compiler_family",
    "compiler_major",
    "compiler_path",
    "compiler_version",
    "case",
    "case_family",
    "pack_size",
    "repeat",
    "status",
    "reason",
    "syntax_wall_seconds",
    "syntax_user_seconds",
    "syntax_system_seconds",
    "syntax_peak_rss_kib",
    "syntax_exit_status",
    "object_wall_seconds",
    "object_user_seconds",
    "object_system_seconds",
    "object_peak_rss_kib",
    "object_exit_status",
    "object_bytes",
    "text_bytes",
    "rodata_bytes",
    "defined_symbol_count",
    "dce_oracle_symbol",
    "dce_oracle_symbol_type",
    "source_sha256",
    "syntax_command_sha256",
    "syntax_command_json",
    "object_command_sha256",
    "object_command_json",
    "diagnostic_excerpt",
    "artifact_directory",
)


@dataclasses.dataclass(frozen=True)
class CompilerSpec:
    label: str
    family: str
    major: int
    command: str
    explicit_override: bool = False


@dataclasses.dataclass(frozen=True)
class CompilerProbe:
    status: str
    reason: str
    path: pathlib.Path | None
    version: str


@dataclasses.dataclass(frozen=True)
class Revision:
    label: str
    requested_root: pathlib.Path
    include_root: pathlib.Path | None
    error: str


@dataclasses.dataclass(frozen=True)
class CompileCase:
    case_id: str
    family: str
    pack_size: int | None
    source_factory: Callable[[], str] | None = None
    extension_slot: str | None = None
    builtin_oracle_symbol: str | None = None


@dataclasses.dataclass(frozen=True)
class CapturedProcess:
    returncode: int
    stdout: str
    stderr: str
    timed_out: bool


@dataclasses.dataclass(frozen=True)
class ToolPaths:
    time: pathlib.Path | None
    taskset: pathlib.Path | None
    size: pathlib.Path | None
    nm: pathlib.Path | None
    error: str


def utc_timestamp() -> str:
    return dt.datetime.now(dt.timezone.utc).isoformat(timespec="milliseconds")


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def normalized_excerpt(value: str, limit: int = 2000) -> str:
    """Keep diagnostics useful while preserving one compact logical CSV cell."""

    return " ".join(value.split())[:limit]


def noinline_prefix() -> str:
    return """\
#if defined(__GNUC__) || defined(__clang__)
#define FAST_IO_COMPILE_COST_NOINLINE __attribute__((noinline))
#else
#define FAST_IO_COMPILE_COST_NOINLINE
#endif
"""


def render_print_pack(pack_size: int) -> str:
    arguments = ",\n\t\t".join(
        f"static_cast<::std::uint_least64_t>(value + {index}u)"
        for index in range(pack_size)
    )
    return f"""\
/*
 * This generated translation unit instantiates exactly one public print pack.
 * The opaque memory boundary makes every emitted byte observable, preventing
 * dead-store elimination from changing the code-generation comparison.
 */
#include <cstddef>
#include <cstdint>
#include <fast_io.h>

{noinline_prefix()}

extern "C" FAST_IO_COMPILE_COST_NOINLINE ::std::size_t
fast_io_compile_cost_case(::std::uint_least64_t value)
{{
\tchar storage[4096u];
\t::fast_io::basic_obuffer_view<char> output{{storage, storage + sizeof(storage)}};
\t::fast_io::operations::print_freestanding<false>(
\t\toutput,
\t\t{arguments});
#if defined(__GNUC__) || defined(__clang__)
\t__asm__ __volatile__("" : : "m"(storage) : "memory");
#endif
\tauto const size{{output.size()}};
\treturn size ^ static_cast<unsigned char>(storage[0u]) ^
\t\t   (static_cast<::std::size_t>(static_cast<unsigned char>(storage[size - 1u])) << 8u);
}}
"""


def render_concat_pack(pack_size: int) -> str:
    arguments = ",\n\t\t".join(
        f"static_cast<::std::uint_least64_t>(value + {index}u)"
        for index in range(pack_size)
    )
    return f"""\
/*
 * This generated translation unit instantiates exactly one native-string
 * concat pack.  Escaping both the base and extent preserves full result
 * materialization under optimization.
 */
#include <cstddef>
#include <cstdint>
#include <fast_io_dsal/string.h>
#include <fast_io.h>

{noinline_prefix()}

extern "C" FAST_IO_COMPILE_COST_NOINLINE ::std::size_t
fast_io_compile_cost_case(::std::uint_least64_t value)
{{
\tauto result{{::fast_io::concat_fast_io(
\t\t{arguments})}};
#if defined(__GNUC__) || defined(__clang__)
\t__asm__ __volatile__("" : : "r"(result.data()), "r"(result.size()) : "memory");
#endif
\tauto const size{{result.size()}};
\treturn size ^ static_cast<unsigned char>(result.front()) ^
\t\t   (static_cast<::std::size_t>(static_cast<unsigned char>(result.back())) << 8u);
}}
"""


def render_scan_scalar() -> str:
    return f"""\
/*
 * This translation unit measures the public reporting scan front door.  Both
 * the cursor transition and parsed value cross the external boundary, so the
 * optimizer must retain normalization and the complete input/target state
 * machine rather than only its decay implementation.
 */
#include <cstddef>
#include <cstdint>
#include <fast_io.h>

{noinline_prefix()}

extern "C" FAST_IO_COMPILE_COST_NOINLINE ::std::uint_least64_t
fast_io_compile_cost_case(char const *first, char const *last)
{{
\t::std::uint_least64_t value{{}};
\t::fast_io::basic_ibuffer_view<char> input{{first, last}};
\tauto const success{{::fast_io::io::scan<true>(input, value)}};
\treturn value ^
\t\t   (static_cast<::std::uint_least64_t>(input.curr_ptr - first) << 48u) ^
\t\t   (static_cast<::std::uint_least64_t>(success) << 63u);
}}
"""


def render_transmit_all() -> str:
    return f"""\
/*
 * This translation unit instantiates one exact-count input-to-output transfer.
 * External ranges make the read/write effects observable while avoiding any
 * runtime fixture in the compile-only object.
 */
#include <cstddef>
#include <fast_io.h>

{noinline_prefix()}

extern "C" FAST_IO_COMPILE_COST_NOINLINE ::std::size_t
fast_io_compile_cost_case(char *output_first, char *output_last,
\t\t\t\t\t\t  char const *input_first, char const *input_last)
{{
\t::fast_io::basic_obuffer_view<char> output{{output_first, output_last}};
\t::fast_io::basic_ibuffer_view<char> input{{input_first, input_last}};
\tauto const count{{static_cast<::fast_io::uintfpos_t>(input_last - input_first)}};
\t::fast_io::operations::transmit_all(output, input, count);
\treturn output.size() ^ static_cast<::std::size_t>(input.curr_ptr - input_first);
}}
"""


def builtin_cases() -> tuple[CompileCase, ...]:
    result: list[CompileCase] = []
    for pack_size in (1, 8, 32):
        result.append(
            CompileCase(
                f"print_pack_{pack_size}",
                "print",
                pack_size,
                lambda pack_size=pack_size: render_print_pack(pack_size),
                builtin_oracle_symbol="fast_io_compile_cost_case",
            )
        )
    for pack_size in (1, 8, 32):
        result.append(
            CompileCase(
                f"concat_pack_{pack_size}",
                "concat",
                pack_size,
                lambda pack_size=pack_size: render_concat_pack(pack_size),
                builtin_oracle_symbol="fast_io_compile_cost_case",
            )
        )
    result.extend(
        (
            CompileCase(
                "scan_scalar", "scan", None, render_scan_scalar,
                builtin_oracle_symbol="fast_io_compile_cost_case"
            ),
            CompileCase(
                "transmit_all", "transmit", None, render_transmit_all,
                builtin_oracle_symbol="fast_io_compile_cost_case"
            ),
        )
    )
    # Extension slots are deliberate matrix entries.  An absent source is an
    # explicit SKIP, whereas a declared source that fails to compile is a FAIL.
    for slot in EXTENSION_SLOTS:
        result.append(
            CompileCase(
                f"{slot}_extension",
                slot,
                None,
                source_factory=None,
                extension_slot=slot,
            )
        )
    return tuple(result)


def default_compilers() -> tuple[CompilerSpec, ...]:
    result: list[CompilerSpec] = []
    for major in range(11, 17):
        result.append(CompilerSpec(f"gcc{major}", "gcc", major, f"g++-{major}"))
    for major in range(17, 24):
        result.append(
            CompilerSpec(f"clang{major}", "clang", major, f"clang++-{major}")
        )
    return tuple(result)


def parse_key_value(value: str, option_name: str) -> tuple[str, str]:
    key, separator, item = value.partition("=")
    if not separator or not key or not item:
        raise ValueError(f"{option_name} requires LABEL=VALUE, received {value!r}")
    return key, item


def validate_label(label: str, description: str) -> None:
    if not LABEL_PATTERN.fullmatch(label):
        raise ValueError(
            f"invalid {description} {label!r}; use letters, digits, '.', '_' or '-'"
        )


def normalize_include_root(requested: pathlib.Path) -> tuple[pathlib.Path | None, str]:
    root = requested.expanduser().resolve(strict=False)
    direct_header = root / "fast_io.h"
    repository_header = root / "include" / "fast_io.h"
    if direct_header.is_file():
        return root, ""
    if repository_header.is_file():
        return root / "include", ""
    return None, f"include_root_missing_fast_io_h:{root}"


def parse_revisions(values: Sequence[str]) -> list[Revision]:
    raw_values = list(values)
    if not raw_values:
        raw_values = [
            f"old={REPOSITORY_ROOT.parent / 'fast_io'}",
            f"new={REPOSITORY_ROOT}",
        ]
    result: list[Revision] = []
    seen: set[str] = set()
    for value in raw_values:
        label, root_text = parse_key_value(value, "--include-root")
        validate_label(label, "revision label")
        if label in seen:
            raise ValueError(f"duplicate revision label {label!r}")
        seen.add(label)
        requested = pathlib.Path(root_text).expanduser().resolve(strict=False)
        include_root, error = normalize_include_root(requested)
        result.append(Revision(label, requested, include_root, error))
    return result


def parse_extensions(
    values: Sequence[str], revision_labels: set[str]
) -> dict[tuple[str, str | None], pathlib.Path]:
    result: dict[tuple[str, str | None], pathlib.Path] = {}
    for value in values:
        selector, source_text = parse_key_value(value, "--extension-source")
        slot, marker, revision = selector.partition("@")
        if slot not in EXTENSION_SLOTS:
            raise ValueError(
                f"unknown extension slot {slot!r}; expected one of {', '.join(EXTENSION_SLOTS)}"
            )
        revision_key: str | None = revision if marker else None
        if marker and not revision:
            raise ValueError(f"empty revision selector in {value!r}")
        if revision_key is not None and revision_key not in revision_labels:
            raise ValueError(
                f"extension source selects unknown revision {revision_key!r}"
            )
        key = (slot, revision_key)
        if key in result:
            raise ValueError(f"duplicate extension source for {selector!r}")
        result[key] = pathlib.Path(source_text).expanduser().resolve(strict=False)
    return result


def parse_extension_symbols(
    values: Sequence[str], revision_labels: set[str]
) -> dict[tuple[str, str | None], str]:
    """Parse exact, unmangled symbols that prove extension code survived DCE."""

    result: dict[tuple[str, str | None], str] = {}
    for value in values:
        selector, symbol = parse_key_value(value, "--extension-symbol")
        slot, marker, revision = selector.partition("@")
        if slot not in EXTENSION_SLOTS:
            raise ValueError(
                f"unknown extension slot {slot!r}; expected one of {', '.join(EXTENSION_SLOTS)}"
            )
        revision_key: str | None = revision if marker else None
        if marker and not revision:
            raise ValueError(f"empty revision selector in {value!r}")
        if revision_key is not None and revision_key not in revision_labels:
            raise ValueError(
                f"extension symbol selects unknown revision {revision_key!r}"
            )
        if not EXTERNAL_SYMBOL_PATTERN.fullmatch(symbol):
            raise ValueError(
                f"invalid external symbol {symbol!r}; declare an unmangled C identifier"
            )
        key = (slot, revision_key)
        if key in result:
            raise ValueError(f"duplicate extension symbol for {selector!r}")
        result[key] = symbol
    return result


def apply_compiler_overrides(
    specs: Sequence[CompilerSpec], values: Sequence[str]
) -> tuple[CompilerSpec, ...]:
    by_label = {spec.label: spec for spec in specs}
    overrides: dict[str, str] = {}
    for value in values:
        label, command = parse_key_value(value, "--compiler")
        if label not in by_label:
            raise ValueError(f"unknown compiler label {label!r}")
        if label in overrides:
            raise ValueError(f"duplicate compiler override for {label!r}")
        overrides[label] = command
    return tuple(
        dataclasses.replace(
            spec,
            command=overrides.get(spec.label, spec.command),
            explicit_override=spec.label in overrides,
        )
        for spec in specs
    )


def resolve_program(command: str) -> pathlib.Path | None:
    expanded = pathlib.Path(command).expanduser()
    if expanded.parent != pathlib.Path(".") or os.sep in command:
        resolved = expanded.resolve(strict=False)
        return resolved if resolved.is_file() and os.access(resolved, os.X_OK) else None
    found = shutil.which(command)
    return pathlib.Path(found).resolve() if found else None


def run_captured(
    command: Sequence[str], *, timeout: float, environment: dict[str, str]
) -> CapturedProcess:
    """Run one process group so a timeout cannot leave compiler children alive."""

    process = subprocess.Popen(
        list(command),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        errors="replace",
        env=environment,
        start_new_session=True,
    )
    try:
        stdout, stderr = process.communicate(timeout=timeout)
        return CapturedProcess(process.returncode, stdout, stderr, False)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        stdout, stderr = process.communicate()
        return CapturedProcess(process.returncode, stdout, stderr, True)


def probe_compiler(
    spec: CompilerSpec, environment: dict[str, str]
) -> CompilerProbe:
    path = resolve_program(spec.command)
    if path is None:
        status = "FAIL" if spec.explicit_override else "SKIP"
        reason = (
            "explicit_compiler_unavailable"
            if spec.explicit_override
            else "default_compiler_unavailable"
        )
        return CompilerProbe(status, reason, None, "")
    try:
        version_process = run_captured(
            (str(path), "--version"),
            timeout=PROBE_TIMEOUT_SECONDS,
            environment=environment,
        )
        dump_process = run_captured(
            (str(path), "-dumpversion"),
            timeout=PROBE_TIMEOUT_SECONDS,
            environment=environment,
        )
    except OSError as error:
        return CompilerProbe("FAIL", f"compiler_probe_os_error:{error}", path, "")
    if version_process.timed_out or dump_process.timed_out:
        return CompilerProbe("FAIL", "compiler_probe_timeout", path, "")
    if version_process.returncode != 0 or dump_process.returncode != 0:
        return CompilerProbe("FAIL", "compiler_probe_nonzero_exit", path, "")
    version_lines = [
        line.strip()
        for line in (version_process.stdout + version_process.stderr).splitlines()
        if line.strip()
    ]
    version = version_lines[0] if version_lines else ""
    dump = dump_process.stdout.strip()
    match = re.match(r"(\d+)", dump)
    if match is None:
        return CompilerProbe("FAIL", f"compiler_major_unparseable:{dump}", path, version)
    actual_major = int(match.group(1))
    if actual_major != spec.major:
        return CompilerProbe(
            "FAIL",
            f"compiler_major_mismatch:expected={spec.major}:actual={actual_major}",
            path,
            version,
        )
    if spec.family == "clang" and "clang" not in version.lower():
        return CompilerProbe("FAIL", "compiler_family_mismatch:expected=clang", path, version)
    if spec.family == "gcc" and "clang" in version.lower():
        return CompilerProbe("FAIL", "compiler_family_mismatch:expected=gcc", path, version)
    return CompilerProbe("PASS", "", path, version)


def parse_cpu_list(value: str) -> set[int]:
    result: set[int] = set()
    for component in value.split(","):
        first_text, separator, last_text = component.partition("-")
        first = int(first_text)
        last = int(last_text) if separator else first
        if last < first:
            raise ValueError(f"descending CPU range {component!r} is invalid")
        result.update(range(first, last + 1))
    return result


def resolve_tools(args: argparse.Namespace, environment: dict[str, str]) -> ToolPaths:
    time_path = resolve_program(args.time_command)
    size_path = resolve_program(args.size_command)
    nm_path = resolve_program(args.nm_command)
    taskset_path = None if args.cpu == "none" else resolve_program(args.taskset_command)
    errors: list[str] = []
    if time_path is None:
        errors.append("gnu_time_unavailable")
    else:
        try:
            probe = run_captured(
                (str(time_path), "--version"),
                timeout=PROBE_TIMEOUT_SECONDS,
                environment=environment,
            )
            banner = (probe.stdout + probe.stderr).lower()
            if probe.timed_out or probe.returncode != 0 or "gnu time" not in banner:
                errors.append("time_command_is_not_gnu_time")
        except OSError as error:
            errors.append(f"gnu_time_probe_os_error:{error}")
    if size_path is None:
        errors.append("size_tool_unavailable")
    if nm_path is None:
        errors.append("nm_tool_unavailable")
    if args.cpu != "none" and taskset_path is None:
        errors.append("taskset_unavailable")
    if args.cpu != "none" and hasattr(os, "sched_getaffinity"):
        requested = parse_cpu_list(args.cpu)
        unavailable = requested.difference(os.sched_getaffinity(0))
        if unavailable:
            errors.append(
                "cpu_outside_process_affinity:" + ",".join(map(str, sorted(unavailable)))
            )
    return ToolPaths(time_path, taskset_path, size_path, nm_path, ";".join(errors))


def parse_time_metrics(path: pathlib.Path) -> tuple[dict[str, str], str]:
    try:
        content = path.read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        return {}, f"time_metrics_unreadable:{error}"
    values: dict[str, str] = {}
    for line in content.splitlines():
        key, separator, value = line.partition("=")
        if separator:
            values[key.strip()] = value.strip()
    required = ("wall", "user", "system", "peak_rss_kib", "exit_status")
    missing = [key for key in required if key not in values]
    if missing:
        return values, "time_metrics_missing:" + ",".join(missing)
    try:
        for key in ("wall", "user", "system"):
            if float(values[key]) < 0.0:
                raise ValueError(f"negative {key}")
        if int(values["peak_rss_kib"]) < 0 or int(values["exit_status"]) < 0:
            raise ValueError("negative integer metric")
    except ValueError as error:
        return values, f"time_metrics_invalid:{error}"
    return values, ""


def measure_object(
    object_path: pathlib.Path,
    tools: ToolPaths,
    environment: dict[str, str],
    timeout: float,
    required_symbol: str,
) -> tuple[dict[str, int | str], str]:
    if not object_path.is_file():
        return {}, "object_not_produced"
    try:
        object_bytes = object_path.stat().st_size
    except OSError as error:
        return {}, f"object_stat_failed:{error}"
    if object_bytes <= 0:
        return {}, "object_is_empty"
    assert tools.size is not None and tools.nm is not None
    try:
        size_process = run_captured(
            (str(tools.size), "-A", "-d", str(object_path)),
            timeout=timeout,
            environment=environment,
        )
        nm_process = run_captured(
            (
                str(tools.nm),
                "-a",
                "--defined-only",
                "--format=posix",
                str(object_path),
            ),
            timeout=timeout,
            environment=environment,
        )
    except OSError as error:
        return {"object_bytes": object_bytes}, f"object_tool_os_error:{error}"
    if size_process.timed_out or nm_process.timed_out:
        return {"object_bytes": object_bytes}, "object_tool_timeout"
    if size_process.returncode != 0:
        return {
            "object_bytes": object_bytes
        }, f"size_tool_exit_{size_process.returncode}:{normalized_excerpt(size_process.stderr)}"
    if nm_process.returncode != 0:
        return {
            "object_bytes": object_bytes
        }, f"nm_tool_exit_{nm_process.returncode}:{normalized_excerpt(nm_process.stderr)}"
    text_bytes = 0
    rodata_bytes = 0
    parsed_section = False
    for line in size_process.stdout.splitlines():
        fields = line.split()
        if len(fields) < 3:
            continue
        name = fields[0]
        try:
            size = int(fields[1], 10)
        except ValueError:
            continue
        if not name.startswith("."):
            continue
        parsed_section = True
        if name == ".text" or name.startswith(".text."):
            text_bytes += size
        elif name == ".rodata" or name.startswith(".rodata."):
            rodata_bytes += size
    if not parsed_section:
        return {"object_bytes": object_bytes}, "size_tool_output_unparseable"
    symbol_count = 0
    oracle_symbol_type = ""
    for line in nm_process.stdout.splitlines():
        fields = line.split()
        if not fields or line.rstrip().endswith(":"):
            continue
        symbol_count += 1
        if fields[0] == required_symbol and len(fields) >= 2:
            oracle_symbol_type = fields[1]
    # An exact external text symbol is the object-pass oracle.  This rejects a
    # nominally successful extension TU whose interesting template path was
    # optimized away, internalized, or replaced by an unrelated data symbol.
    if not oracle_symbol_type:
        return {
            "object_bytes": object_bytes,
            "text_bytes": text_bytes,
            "rodata_bytes": rodata_bytes,
            "defined_symbol_count": symbol_count,
        }, f"dce_oracle_symbol_missing:{required_symbol}"
    if oracle_symbol_type not in ("T", "W"):
        return {
            "object_bytes": object_bytes,
            "text_bytes": text_bytes,
            "rodata_bytes": rodata_bytes,
            "defined_symbol_count": symbol_count,
            "dce_oracle_symbol_type": oracle_symbol_type,
        }, f"dce_oracle_symbol_not_external_text:{required_symbol}:{oracle_symbol_type}"
    return (
        {
            "object_bytes": object_bytes,
            "text_bytes": text_bytes,
            "rodata_bytes": rodata_bytes,
            "defined_symbol_count": symbol_count,
            "dce_oracle_symbol_type": oracle_symbol_type,
        },
        "",
    )


def blank_row(
    run_id: str,
    sequence: int,
    revision: Revision,
    spec: CompilerSpec,
    probe: CompilerProbe,
    case: CompileCase,
    repeat: int,
) -> dict[str, object]:
    row: dict[str, object] = {field: "" for field in CSV_FIELDS}
    row.update(
        {
            "schema_version": SCHEMA_VERSION,
            "run_id": run_id,
            "sequence": sequence,
            "host": platform.node(),
            "revision": revision.label,
            "include_root": str(revision.include_root or revision.requested_root),
            "compiler": spec.label,
            "compiler_family": spec.family,
            "compiler_major": spec.major,
            "compiler_path": str(probe.path) if probe.path else "",
            "compiler_version": probe.version,
            "case": case.case_id,
            "case_family": case.family,
            "pack_size": case.pack_size if case.pack_size is not None else "",
            "repeat": repeat,
        }
    )
    return row


def resolve_extension_source(
    case: CompileCase,
    revision: Revision,
    extensions: dict[tuple[str, str | None], pathlib.Path],
) -> pathlib.Path | None:
    assert case.extension_slot is not None
    return extensions.get(
        (case.extension_slot, revision.label),
        extensions.get((case.extension_slot, None)),
    )


def resolve_extension_symbol(
    case: CompileCase,
    revision: Revision,
    extension_symbols: dict[tuple[str, str | None], str],
) -> str | None:
    assert case.extension_slot is not None
    return extension_symbols.get(
        (case.extension_slot, revision.label),
        extension_symbols.get((case.extension_slot, None)),
    )


def build_timed_prefix(
    args: argparse.Namespace,
    tools: ToolPaths,
    time_path: pathlib.Path,
) -> list[str]:
    """Build the measurement wrapper shared by the two compiler processes."""

    assert tools.time is not None
    command = [str(tools.time), "-f", GNU_TIME_FORMAT, "-o", str(time_path)]
    if args.cpu != "none":
        assert tools.taskset is not None
        command.extend((str(tools.taskset), "-c", args.cpu))
    return command


def append_common_compile_options(
    command: list[str],
    args: argparse.Namespace,
    compiler_path: pathlib.Path,
    include_root: pathlib.Path,
    extension_origin: pathlib.Path | None,
) -> None:
    command.extend(
        (
            str(compiler_path),
            f"-std={args.standard}",
            "-DNDEBUG",
            "-fdiagnostics-color=never",
            "-I",
            str(include_root),
        )
    )
    if extension_origin is not None:
        # A copied extension TU retains quoted-include semantics through its
        # original directory without sharing an object or compiler process.
        command.extend(("-iquote", str(extension_origin.parent)))


def build_syntax_command(
    args: argparse.Namespace,
    tools: ToolPaths,
    compiler_path: pathlib.Path,
    include_root: pathlib.Path,
    source_path: pathlib.Path,
    time_path: pathlib.Path,
    extension_origin: pathlib.Path | None,
) -> list[str]:
    """Build a parsing/instantiation pass with no optimizer or object backend."""

    command = build_timed_prefix(args, tools, time_path)
    append_common_compile_options(
        command, args, compiler_path, include_root, extension_origin
    )
    command.extend(args.cxxflag)
    command.extend(args.syntax_flag)
    command.extend(("-fsyntax-only", str(source_path)))
    return command


def build_object_command(
    args: argparse.Namespace,
    tools: ToolPaths,
    compiler_path: pathlib.Path,
    include_root: pathlib.Path,
    source_path: pathlib.Path,
    object_path: pathlib.Path,
    time_path: pathlib.Path,
    extension_origin: pathlib.Path | None,
) -> list[str]:
    """Build the separately timed optimization, code-generation and assembly pass."""

    command = build_timed_prefix(args, tools, time_path)
    append_common_compile_options(
        command, args, compiler_path, include_root, extension_origin
    )
    command.extend(
        (
            f"-{args.optimization}",
            "-ffunction-sections",
            "-fdata-sections",
        )
    )
    command.extend(args.cxxflag)
    command.extend(args.object_flag)
    command.extend(("-c", str(source_path), "-o", str(object_path)))
    return command


def revision_order_for_repeat(
    revisions: Sequence[Revision], repeat: int
) -> tuple[Revision, ...]:
    """Keep revisions adjacent and reverse even repeats to balance order bias."""

    ordered = tuple(revisions)
    return ordered if repeat % 2 == 1 else tuple(reversed(ordered))


def record_pass_metrics(
    row: dict[str, object], prefix: str, values: dict[str, str]
) -> None:
    row.update(
        {
            f"{prefix}_wall_seconds": values.get("wall", ""),
            f"{prefix}_user_seconds": values.get("user", ""),
            f"{prefix}_system_seconds": values.get("system", ""),
            f"{prefix}_peak_rss_kib": values.get("peak_rss_kib", ""),
            f"{prefix}_exit_status": values.get("exit_status", ""),
        }
    )


def pass_failure_reason(
    prefix: str,
    process: CapturedProcess,
    time_values: dict[str, str],
    time_error: str,
) -> str:
    if process.timed_out:
        return f"{prefix}_timeout"
    if process.returncode != 0:
        return f"{prefix}_exit_{process.returncode}"
    if time_error:
        return f"{prefix}_{time_error}"
    if time_values.get("exit_status") != "0":
        return f"{prefix}_time_exit_status_disagrees_with_process"
    return ""


def write_row(writer: csv.DictWriter, stream: object, row: dict[str, object]) -> None:
    row["timestamp_utc"] = utc_timestamp()
    writer.writerow(row)
    # A complete row is a recovery boundary if a long compiler matrix is
    # interrupted after earlier samples have already succeeded.
    stream.flush()  # type: ignore[attr-defined]


def execute_matrix(
    args: argparse.Namespace,
    revisions: Sequence[Revision],
    compiler_specs: Sequence[CompilerSpec],
    cases: Sequence[CompileCase],
    extensions: dict[tuple[str, str | None], pathlib.Path],
    extension_symbols: dict[tuple[str, str | None], str],
) -> int:
    environment = os.environ.copy()
    environment["LC_ALL"] = "C"
    environment["LANG"] = "C"
    tools = resolve_tools(args, environment)
    probes = {spec.label: probe_compiler(spec, environment) for spec in compiler_specs}
    run_id = uuid.uuid4().hex
    retained = args.artifact_root is not None
    temporary: tempfile.TemporaryDirectory[str] | None = None
    if retained:
        work_root = pathlib.Path(args.artifact_root).expanduser().resolve() / run_id
        work_root.mkdir(parents=True, exist_ok=False)
    else:
        temporary = tempfile.TemporaryDirectory(prefix="fast_io_compile_cost_")
        work_root = pathlib.Path(temporary.name)
    output_path = pathlib.Path(args.output).expanduser().resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    mode = "w" if args.overwrite else "x"
    counts = {"PASS": 0, "FAIL": 0, "SKIP": 0}
    sequence = 0
    try:
        with output_path.open(mode, newline="", encoding="utf-8") as csv_stream:
            writer = csv.DictWriter(csv_stream, fieldnames=CSV_FIELDS)
            writer.writeheader()
            csv_stream.flush()
            for spec in compiler_specs:
                probe = probes[spec.label]
                for case in cases:
                    for repeat in range(1, args.repeat + 1):
                        # Each old/new sample remains adjacent.  Reversing the
                        # pair on even repeats distributes cache, thermal and
                        # background-load bias across both revisions.
                        for revision in revision_order_for_repeat(revisions, repeat):
                            sequence += 1
                            row = blank_row(
                                run_id, sequence, revision, spec, probe, case, repeat
                            )
                            # A malformed revision is a global configuration
                            # failure.  It takes precedence over an optional
                            # case being absent so the matrix cannot conceal a
                            # bad include root behind extension SKIPs.
                            if revision.error:
                                row.update(status="FAIL", reason=revision.error)
                                counts["FAIL"] += 1
                                write_row(writer, csv_stream, row)
                                continue
                            extension_origin: pathlib.Path | None = None
                            extension_source: pathlib.Path | None = None
                            oracle_symbol = case.builtin_oracle_symbol
                            if case.extension_slot is not None:
                                extension_source = resolve_extension_source(
                                    case, revision, extensions
                                )
                                oracle_symbol = resolve_extension_symbol(
                                    case, revision, extension_symbols
                                )
                                if extension_source is None:
                                    row.update(
                                        status="SKIP",
                                        reason="optional_extension_source_not_supplied",
                                    )
                                    counts["SKIP"] += 1
                                    write_row(writer, csv_stream, row)
                                    continue
                                if not extension_source.is_file():
                                    row.update(
                                        status="FAIL",
                                        reason=f"declared_extension_source_missing:{extension_source}",
                                    )
                                    counts["FAIL"] += 1
                                    write_row(writer, csv_stream, row)
                                    continue
                                if oracle_symbol is None:
                                    row.update(
                                        status="FAIL",
                                        reason="extension_dce_oracle_symbol_not_supplied",
                                    )
                                    counts["FAIL"] += 1
                                    write_row(writer, csv_stream, row)
                                    continue
                                extension_origin = extension_source
                            assert oracle_symbol is not None
                            row["dce_oracle_symbol"] = oracle_symbol
                            if probe.status != "PASS":
                                row.update(status=probe.status, reason=probe.reason)
                                counts[probe.status] += 1
                                write_row(writer, csv_stream, row)
                                continue
                            if tools.error:
                                row.update(status="FAIL", reason=tools.error)
                                counts["FAIL"] += 1
                                write_row(writer, csv_stream, row)
                                continue
                            assert revision.include_root is not None and probe.path is not None
                            case_directory = work_root / (
                                f"{sequence:05d}_{spec.label}_{case.case_id}_"
                                f"repeat{repeat}_{revision.label}"
                            )
                            case_directory.mkdir(parents=True, exist_ok=False)
                            source_path = case_directory / "case.cc"
                            object_path = case_directory / "case.o"
                            syntax_time_path = case_directory / "syntax_time.txt"
                            syntax_stdout_path = case_directory / "syntax_stdout.txt"
                            syntax_stderr_path = case_directory / "syntax_stderr.txt"
                            object_time_path = case_directory / "object_time.txt"
                            object_stdout_path = case_directory / "object_stdout.txt"
                            object_stderr_path = case_directory / "object_stderr.txt"
                            try:
                                if extension_source is not None:
                                    source_bytes = extension_source.read_bytes()
                                else:
                                    assert case.source_factory is not None
                                    source_bytes = case.source_factory().encode("utf-8")
                                source_path.write_bytes(source_bytes)
                            except OSError as error:
                                row.update(
                                    status="FAIL", reason=f"source_materialization_failed:{error}"
                                )
                                counts["FAIL"] += 1
                                write_row(writer, csv_stream, row)
                                continue
                            syntax_command = build_syntax_command(
                                args,
                                tools,
                                probe.path,
                                revision.include_root,
                                source_path,
                                syntax_time_path,
                                extension_origin,
                            )
                            object_command = build_object_command(
                                args,
                                tools,
                                probe.path,
                                revision.include_root,
                                source_path,
                                object_path,
                                object_time_path,
                                extension_origin,
                            )
                            syntax_command_json = json.dumps(
                                syntax_command, ensure_ascii=False
                            )
                            object_command_json = json.dumps(
                                object_command, ensure_ascii=False
                            )
                            row.update(
                                source_sha256=sha256_bytes(source_bytes),
                                syntax_command_sha256=sha256_bytes(
                                    syntax_command_json.encode("utf-8")
                                ),
                                syntax_command_json=syntax_command_json,
                                object_command_sha256=sha256_bytes(
                                    object_command_json.encode("utf-8")
                                ),
                                object_command_json=object_command_json,
                                artifact_directory=str(case_directory) if retained else "",
                            )
                            diagnostics: list[str] = []
                            try:
                                syntax_process = run_captured(
                                    syntax_command,
                                    timeout=args.timeout_seconds,
                                    environment=environment,
                                )
                            except OSError as error:
                                row.update(
                                    status="FAIL",
                                    reason=f"syntax_process_os_error:{error}",
                                )
                                counts["FAIL"] += 1
                                write_row(writer, csv_stream, row)
                                continue
                            syntax_stdout_path.write_text(
                                syntax_process.stdout,
                                encoding="utf-8",
                                errors="replace",
                            )
                            syntax_stderr_path.write_text(
                                syntax_process.stderr,
                                encoding="utf-8",
                                errors="replace",
                            )
                            diagnostics.append(
                                "[syntax] "
                                + syntax_process.stdout
                                + "\n"
                                + syntax_process.stderr
                            )
                            syntax_values, syntax_time_error = parse_time_metrics(
                                syntax_time_path
                            )
                            record_pass_metrics(row, "syntax", syntax_values)
                            syntax_error = pass_failure_reason(
                                "syntax",
                                syntax_process,
                                syntax_values,
                                syntax_time_error,
                            )
                            if syntax_error:
                                row.update(
                                    status="FAIL",
                                    reason=syntax_error,
                                    diagnostic_excerpt=normalized_excerpt(
                                        "\n".join(diagnostics)
                                    ),
                                )
                                counts["FAIL"] += 1
                                write_row(writer, csv_stream, row)
                                continue
                            try:
                                object_process = run_captured(
                                    object_command,
                                    timeout=args.timeout_seconds,
                                    environment=environment,
                                )
                            except OSError as error:
                                row.update(
                                    status="FAIL",
                                    reason=f"object_process_os_error:{error}",
                                    diagnostic_excerpt=normalized_excerpt(
                                        "\n".join(diagnostics)
                                    ),
                                )
                                counts["FAIL"] += 1
                                write_row(writer, csv_stream, row)
                                continue
                            object_stdout_path.write_text(
                                object_process.stdout,
                                encoding="utf-8",
                                errors="replace",
                            )
                            object_stderr_path.write_text(
                                object_process.stderr,
                                encoding="utf-8",
                                errors="replace",
                            )
                            diagnostics.append(
                                "[object] "
                                + object_process.stdout
                                + "\n"
                                + object_process.stderr
                            )
                            row["diagnostic_excerpt"] = normalized_excerpt(
                                "\n".join(diagnostics)
                            )
                            object_time_values, object_time_error = parse_time_metrics(
                                object_time_path
                            )
                            record_pass_metrics(row, "object", object_time_values)
                            object_error = pass_failure_reason(
                                "object",
                                object_process,
                                object_time_values,
                                object_time_error,
                            )
                            if object_error:
                                row.update(status="FAIL", reason=object_error)
                            else:
                                object_values, measurement_error = measure_object(
                                    object_path,
                                    tools,
                                    environment,
                                    args.timeout_seconds,
                                    oracle_symbol,
                                )
                                row.update(object_values)
                                if measurement_error:
                                    row.update(status="FAIL", reason=measurement_error)
                                else:
                                    row.update(status="PASS", reason="")
                            counts[str(row["status"])] += 1
                            write_row(writer, csv_stream, row)
    finally:
        if temporary is not None:
            temporary.cleanup()
    print(
        f"wrote {sequence} rows to {output_path}: "
        f"PASS={counts['PASS']} FAIL={counts['FAIL']} SKIP={counts['SKIP']}"
    )
    return 1 if counts["FAIL"] else 0


def dry_run_matrix(
    revisions: Sequence[Revision],
    compiler_specs: Sequence[CompilerSpec],
    cases: Sequence[CompileCase],
    extensions: dict[tuple[str, str | None], pathlib.Path],
    extension_symbols: dict[tuple[str, str | None], str],
    repeat: int,
) -> int:
    """Print the complete plan without probing or invoking any compiler."""

    sequence = 0
    has_failure = False
    for spec in compiler_specs:
        compiler_path = resolve_program(spec.command)
        for case in cases:
            for repeat_index in range(1, repeat + 1):
                for revision in revision_order_for_repeat(revisions, repeat_index):
                    sequence += 1
                    status = "PLAN"
                    reason = ""
                    if revision.error:
                        status = "FAIL"
                        reason = revision.error
                    elif case.extension_slot is not None:
                        source = resolve_extension_source(case, revision, extensions)
                        if source is None:
                            status = "SKIP"
                            reason = "optional_extension_source_not_supplied"
                        elif not source.is_file():
                            status = "FAIL"
                            reason = f"declared_extension_source_missing:{source}"
                        elif resolve_extension_symbol(
                            case, revision, extension_symbols
                        ) is None:
                            status = "FAIL"
                            reason = "extension_dce_oracle_symbol_not_supplied"
                    if compiler_path is None and status == "PLAN":
                        status = "FAIL" if spec.explicit_override else "SKIP"
                        reason = (
                            "explicit_compiler_unavailable"
                            if spec.explicit_override
                            else "default_compiler_unavailable"
                        )
                    has_failure = has_failure or status == "FAIL"
                    print(
                        f"{sequence:04d} {spec.label:8s} {case.case_id:22s} "
                        f"repeat={repeat_index} revision={revision.label} "
                        f"status={status} {reason}"
                    )
    return 1 if has_failure else 0


def make_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Measure isolated old/new fast_io compilation cost on Linux."
    )
    parser.add_argument(
        "--include-root",
        action="append",
        default=[],
        metavar="LABEL=PATH",
        help=(
            "repeatable revision include root; PATH may be a repository root or its include directory "
            "(default: old=../fast_io and new=current repository)"
        ),
    )
    parser.add_argument(
        "--compiler",
        action="append",
        default=[],
        metavar="LABEL=PATH",
        help="override one gcc11..gcc16 or clang17..clang23 compiler executable",
    )
    parser.add_argument(
        "--only-compiler",
        action="append",
        default=[],
        metavar="LABEL",
        help="restrict execution while retaining canonical compiler ordering",
    )
    parser.add_argument(
        "--case",
        action="append",
        default=[],
        metavar="CASE_ID",
        help="restrict execution to one or more listed cases",
    )
    parser.add_argument(
        "--extension-source",
        action="append",
        default=[],
        metavar="SLOT[@REVISION]=PATH",
        help="supply a full TU for the scan, transmit, or transcoder extension slot",
    )
    parser.add_argument(
        "--extension-symbol",
        action="append",
        default=[],
        metavar="SLOT[@REVISION]=SYMBOL",
        help=(
            "required exact extern-C text symbol used as the DCE oracle for an "
            "extension source"
        ),
    )
    parser.add_argument("--cpu", default="16", help="taskset CPU list, or 'none'")
    parser.add_argument("--repeat", type=int, default=1)
    parser.add_argument("--standard", choices=("c++20", "c++23"), default="c++20")
    parser.add_argument(
        "--optimization",
        choices=("O0", "O1", "O2", "O3", "Os", "Oz"),
        default="O2",
    )
    parser.add_argument(
        "--cxxflag",
        action="append",
        default=[],
        help="repeatable extra compiler flag; use --cxxflag=-FLAG syntax",
    )
    parser.add_argument(
        "--syntax-flag",
        action="append",
        default=[],
        help="repeatable flag used only by the -fsyntax-only pass",
    )
    parser.add_argument(
        "--object-flag",
        action="append",
        default=[],
        help="repeatable flag used only by the optimized object pass",
    )
    parser.add_argument(
        "--timeout-seconds",
        type=float,
        default=0.8,
        help="per compiler/tool process timeout (default: 0.8 seconds)",
    )
    parser.add_argument("--time-command", default="/usr/bin/time")
    parser.add_argument("--taskset-command", default="taskset")
    parser.add_argument("--size-command", default="size")
    parser.add_argument("--nm-command", default="nm")
    parser.add_argument("--output", help="new CSV path; required for execution")
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="replace an existing CSV instead of refusing the run",
    )
    parser.add_argument(
        "--artifact-root",
        help="retain unique sources, objects, timing files, and diagnostics below PATH",
    )
    parser.add_argument("--list-cases", action="store_true")
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="print the matrix and classifications without invoking a compiler",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = make_argument_parser()
    args = parser.parse_args(argv)
    all_cases = builtin_cases()
    if args.list_cases:
        for case in all_cases:
            source_kind = "extension" if case.extension_slot else "built-in"
            pack = f" pack={case.pack_size}" if case.pack_size is not None else ""
            print(f"{case.case_id:22s} family={case.family:10s} source={source_kind}{pack}")
        return 0
    try:
        if args.repeat <= 0:
            raise ValueError("--repeat must be positive")
        if args.timeout_seconds <= 0.0:
            raise ValueError("--timeout-seconds must be positive")
        if args.cpu != "none":
            if not CPU_LIST_PATTERN.fullmatch(args.cpu):
                raise ValueError("--cpu must be a taskset list such as 16, 2-3, or none")
            parse_cpu_list(args.cpu)
        revisions = parse_revisions(args.include_root)
        extensions = parse_extensions(
            args.extension_source, {revision.label for revision in revisions}
        )
        extension_symbols = parse_extension_symbols(
            args.extension_symbol, {revision.label for revision in revisions}
        )
        compiler_specs = apply_compiler_overrides(default_compilers(), args.compiler)
        known_compilers = {spec.label for spec in compiler_specs}
        unknown_compilers = set(args.only_compiler).difference(known_compilers)
        if unknown_compilers:
            raise ValueError(
                "unknown --only-compiler labels: " + ",".join(sorted(unknown_compilers))
            )
        if args.only_compiler:
            selected_compilers = set(args.only_compiler)
            compiler_specs = tuple(
                spec for spec in compiler_specs if spec.label in selected_compilers
            )
        known_cases = {case.case_id for case in all_cases}
        unknown_cases = set(args.case).difference(known_cases)
        if unknown_cases:
            raise ValueError("unknown --case IDs: " + ",".join(sorted(unknown_cases)))
        cases = (
            tuple(case for case in all_cases if case.case_id in set(args.case))
            if args.case
            else all_cases
        )
    except ValueError as error:
        parser.error(str(error))
    if args.dry_run:
        return dry_run_matrix(
            revisions,
            compiler_specs,
            cases,
            extensions,
            extension_symbols,
            args.repeat,
        )
    if not sys.platform.startswith("linux"):
        parser.error(
            "compiler execution is Linux-only; use --list-cases or --dry-run on this host"
        )
    if not args.output:
        parser.error("--output is required for execution")
    output_path = pathlib.Path(args.output).expanduser().resolve(strict=False)
    if output_path.exists() and not args.overwrite:
        parser.error(f"output already exists: {output_path}; pass --overwrite explicitly")
    return execute_matrix(
        args, revisions, compiler_specs, cases, extensions, extension_symbols
    )


if __name__ == "__main__":
    raise SystemExit(main())
