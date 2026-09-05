#!/usr/bin/env python3
"""Run the bounded old/new concat ordered-staging regression matrix.

The executable cases stay deliberately small: the main matrix contains exactly
16 translation units, while seven narrowly labelled supplemental units cover
newline handling and the position of one immediate-consumption scatter barrier.
Every build and run is serial.  Darwin artifacts must live below /tmp; Linux
runtime samples require an explicitly selected P-core CPU.
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
import time
import uuid
from collections.abc import Iterable, Sequence


SCRIPT_DIRECTORY = pathlib.Path(__file__).resolve().parent
REPOSITORY_ROOT = SCRIPT_DIRECTORY.parents[1]
DEFAULT_OLD_ROOT = REPOSITORY_ROOT.parent / "fast_io"
DEFAULT_DARWIN_CLANG = pathlib.Path(
    "/Users/liyinan/Documents/MacroModel/tool-chain/tools/"
    "aarch64-apple-darwin-llvm/llvm/bin/clang++"
)
SOURCE_FILES = (
    "concat_ordered_staging_case.cc",
    "byte_oracle.h",
    "case_driver.h",
)
DEFAULT_PROFILES = ("small", "2047", "2048", "2049")
DEFAULT_MAXIMUM_TOTAL_PAYLOAD = 2049
PROFILE_CHOICES = (
    "small", "511", "512", "513", "2047", "2048", "2049",
    "2559", "2560", "2561", "4095", "4096", "4097", "8191", "8192", "8193",
)
SCHEMA_VERSION = "concat-ordered-staging-2"
KERNEL_SYMBOL = "fast_io_concat_ordered_staging_kernel"
RUNTIME_HEADER = (
    "operation",
    "source",
    "topology",
    "pack",
    "line",
    "result",
    "total",
    "seed",
    "iterations",
    "seconds",
    "ns_per_call",
    "checksum",
    "validation_digest",
)
GNU_TIME_FORMAT = "wall=%e\nuser=%U\nsystem=%S\npeak_rss_kib=%M\nexit_status=%x"
FLOAT_PATTERN = r"(?:\d+(?:\.\d*)?|\.\d+)"
DARWIN_TIME_TOTAL_PATTERN = re.compile(
    rf"^\s*({FLOAT_PATTERN})\s+real\s+({FLOAT_PATTERN})\s+user\s+"
    rf"({FLOAT_PATTERN})\s+sys\s*$"
)
DARWIN_TIME_RSS_PATTERN = re.compile(
    r"^\s*(\d+)\s+maximum resident set size\s*$"
)
LINUX_TIME_PATTERN = re.compile(r"^(\w+)=(.*)$")


BUILD_FIELDS = (
    "schema_version",
    "run_id",
    "sequence",
    "timestamp_utc",
    "build_id",
    "group",
    "case",
    "revision",
    "revision_include_sha256",
    "repeat",
    "order_slot",
    "status",
    "reason",
    "host",
    "platform",
    "p_core_cpu",
    "p_core_type",
    "p_core_thread_siblings",
    "compiler_path",
    "compiler_family",
    "compiler_major",
    "compiler_version",
    "standard",
    "sysroot",
    "march",
    "maximum_total_payload",
    "source_sha256",
    "compile_policy_sha256",
    "syntax_wall_seconds",
    "syntax_user_seconds",
    "syntax_system_seconds",
    "syntax_peak_rss_bytes",
    "syntax_exit_status",
    "object_wall_seconds",
    "object_user_seconds",
    "object_system_seconds",
    "object_peak_rss_bytes",
    "object_exit_status",
    "link_wall_seconds",
    "link_user_seconds",
    "link_system_seconds",
    "link_peak_rss_bytes",
    "link_exit_status",
    "object_file_bytes",
    "object_text_bytes",
    "object_rodata_bytes",
    "object_kernel_symbol_type",
    "linked_file_bytes",
    "linked_text_bytes",
    "linked_rodata_bytes",
    "linked_kernel_symbol_type",
    "syntax_command_sha256",
    "syntax_command_json",
    "object_command_sha256",
    "object_command_json",
    "link_command_sha256",
    "link_command_json",
    "diagnostic_excerpt",
    "artifact_directory",
)


RUNTIME_FIELDS = (
    "schema_version",
    "run_id",
    "sequence",
    "timestamp_utc",
    "build_id",
    "group",
    "case",
    "revision",
    "repeat",
    "order_slot",
    "profile",
    "maximum_total_payload",
    "runtime_order",
    "status",
    "reason",
    "host",
    "platform",
    "p_core_cpu",
    "host_load_one",
    "spotlight_cpu_percent",
) + RUNTIME_HEADER + (
    "target_milliseconds",
    "stdout_sha256",
    "runtime_command_sha256",
    "runtime_command_json",
    "diagnostic_excerpt",
    "artifact_directory",
)


@dataclasses.dataclass(frozen=True)
class Case:
    case_id: str
    group: str
    source_id: int
    source_name: str
    pack: int
    result_id: int
    result_name: str
    line: int
    topology_id: int
    topology_name: str

    def definitions(self) -> tuple[str, ...]:
        return (
            f"-DFAST_IO_ORDERED_STAGING_SOURCE={self.source_id}",
            f"-DFAST_IO_ORDERED_STAGING_PACK={self.pack}",
            f"-DFAST_IO_ORDERED_STAGING_RESULT={self.result_id}",
            f"-DFAST_IO_ORDERED_STAGING_LINE={self.line}",
            f"-DFAST_IO_ORDERED_STAGING_TOPOLOGY={self.topology_id}",
        )


@dataclasses.dataclass(frozen=True)
class Revision:
    label: str
    root: pathlib.Path
    include_root: pathlib.Path
    include_sha256: str


@dataclasses.dataclass(frozen=True)
class Compiler:
    path: pathlib.Path
    family: str
    major: int
    version: str


@dataclasses.dataclass(frozen=True)
class ProcessResult:
    returncode: int
    stdout: str
    stderr: str
    timed_out: bool


@dataclasses.dataclass(frozen=True)
class Timing:
    wall: str = ""
    user: str = ""
    system: str = ""
    peak_rss_bytes: str = ""
    exit_status: str = ""


@dataclasses.dataclass(frozen=True)
class BuiltArtifact:
    build_id: str
    case: Case
    revision: Revision
    repeat: int
    order_slot: int
    directory: pathlib.Path
    executable: pathlib.Path
    status: str
    reason: str


def utc_timestamp() -> str:
    return dt.datetime.now(dt.timezone.utc).isoformat(timespec="milliseconds")


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def command_fields(prefix: str, command: Sequence[str]) -> dict[str, str]:
    encoded = json.dumps(list(command), ensure_ascii=False)
    return {
        f"{prefix}_command_json": encoded,
        f"{prefix}_command_sha256": sha256_bytes(encoded.encode("utf-8")),
    }


def normalized_excerpt(value: str, limit: int = 2400) -> str:
    return " ".join(value.split())[:limit]


def matrix_cases() -> tuple[Case, ...]:
    cases: list[Case] = []

    def add(
        group: str,
        source_id: int,
        source_name: str,
        pack: int,
        result_id: int,
        result_name: str,
        line: int = 0,
        topology_id: int = 0,
        topology_name: str = "repeated",
    ) -> None:
        case_id = (
            f"{group}.{source_name}.n{pack}.{result_name}."
            f"line{line}.{topology_name}"
        )
        cases.append(
            Case(
                case_id,
                group,
                source_id,
                source_name,
                pack,
                result_id,
                result_name,
                line,
                topology_id,
                topology_name,
            )
        )

    # The main matrix is intentionally exactly 16 independent translation
    # units.  It surrounds the N=8 policy threshold and retains both negative
    # controls which previously moved in opposite performance directions.
    for pack in (7, 8, 9, 32):
        for result_id, result_name in ((0, "std-string"), (1, "fast-io-string")):
            add("main", 0, "mixed", pack, result_id, result_name)
    for source_id, source_name in ((1, "mixed-borrowed"), (2, "precise")):
        for pack in (8, 32):
            for result_id, result_name in (
                (0, "std-string"),
                (1, "fast-io-string"),
            ):
                add("main", source_id, source_name, pack, result_id, result_name)

    # Newline is rechecked only at the threshold and long-pack points; it is a
    # semantic suffix axis, not a reason to duplicate the complete main grid.
    for pack in (8, 32):
        for result_id, result_name in ((0, "std-string"), (1, "fast-io-string")):
            add(
                "supplemental-line",
                0,
                "mixed",
                pack,
                result_id,
                result_name,
                line=1,
            )

    # One N=8 native-result pack isolates whether staging cost or correctness
    # accidentally depends on the barrier's ordinal position.
    for topology_id, topology_name in ((1, "early"), (2, "middle"), (3, "late")):
        add(
            "supplemental-topology",
            0,
            "mixed",
            8,
            1,
            "fast-io-string",
            topology_id=topology_id,
            topology_name=topology_name,
        )

    assert sum(case.group == "main" for case in cases) == 16
    assert sum(case.group != "main" for case in cases) == 7
    assert len({case.case_id for case in cases}) == 23
    return tuple(cases)


def hash_file_set(paths: Iterable[pathlib.Path], base: pathlib.Path) -> str:
    digest = hashlib.sha256()
    for path in sorted(paths, key=lambda item: item.relative_to(base).as_posix()):
        relative = path.relative_to(base).as_posix().encode("utf-8")
        digest.update(len(relative).to_bytes(8, "little"))
        digest.update(relative)
        content = path.read_bytes()
        digest.update(len(content).to_bytes(8, "little"))
        digest.update(content)
    return digest.hexdigest()


def freeze_sources() -> tuple[dict[str, bytes], str]:
    frozen = {
        name: (SCRIPT_DIRECTORY / name).read_bytes() for name in SOURCE_FILES
    }
    digest = hashlib.sha256()
    for name in SOURCE_FILES:
        encoded_name = name.encode("utf-8")
        digest.update(len(encoded_name).to_bytes(8, "little"))
        digest.update(encoded_name)
        digest.update(len(frozen[name]).to_bytes(8, "little"))
        digest.update(frozen[name])
    return frozen, digest.hexdigest()


def resolve_revision(label: str, value: str) -> Revision:
    root = pathlib.Path(value).expanduser().resolve()
    include_root = root / "include"
    if not (include_root / "fast_io.h").is_file():
        raise ValueError(f"{label} root does not contain include/fast_io.h: {root}")
    files = tuple(path for path in include_root.rglob("*") if path.is_file())
    return Revision(label, root, include_root, hash_file_set(files, include_root))


def run_captured(
    command: Sequence[str], environment: dict[str, str], timeout: float
) -> ProcessResult:
    process = subprocess.Popen(
        list(command),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="replace",
        env=environment,
        start_new_session=True,
    )
    try:
        stdout, stderr = process.communicate(timeout=timeout)
        return ProcessResult(process.returncode, stdout, stderr, False)
    except subprocess.TimeoutExpired:
        # Killing the process group prevents a compiler driver timeout from
        # leaving its cc1 or linker child alive in the single-process slot.
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        stdout, stderr = process.communicate()
        return ProcessResult(process.returncode, stdout, stderr, True)


def sanitized_environment(run_root: pathlib.Path, compiler: pathlib.Path) -> dict[str, str]:
    temporary = run_root / "environment" / "tmp"
    cache = run_root / "environment" / "cache"
    for directory in (temporary, cache):
        directory.mkdir(parents=True, exist_ok=True)
    fixed_path = os.pathsep.join(
        dict.fromkeys(
            (
                str(compiler.parent),
                "/usr/bin",
                "/bin",
                "/usr/sbin",
                "/sbin",
            )
        )
    )
    return {
        "TMPDIR": str(temporary),
        "XDG_CACHE_HOME": str(cache),
        "PATH": fixed_path,
        "LC_ALL": "C",
        "LANG": "C",
        "TZ": "UTC",
        "ZERO_AR_DATE": "1",
    }


def probe_compiler(path: pathlib.Path, environment: dict[str, str]) -> Compiler:
    invocation_path = path.expanduser().absolute()
    canonical_path = invocation_path.resolve()
    if not canonical_path.is_file() or not os.access(invocation_path, os.X_OK):
        raise ValueError(f"compiler is not executable: {invocation_path}")
    result = run_captured(
        (str(invocation_path), "--version"), environment=environment, timeout=10.0
    )
    if result.timed_out or result.returncode != 0:
        raise ValueError("compiler --version failed")
    lines = [line.strip() for line in (result.stdout + result.stderr).splitlines() if line.strip()]
    version = lines[0] if lines else ""
    clang_match = re.search(r"(?:Apple\s+)?clang version\s+(\d+)", version, re.I)
    gcc_match = re.search(r"(?:gcc|g\+\+).*?\s(\d+)(?:\.\d+)+", version, re.I)
    if clang_match:
        # Driver mode is selected from argv[0]. Preserve a clang++/g++ symlink
        # even when its canonical file is named clang-N or gcc-N; resolving it
        # would silently omit the C++ runtime while linking a prebuilt object.
        return Compiler(invocation_path, "clang", int(clang_match.group(1)), version)
    if gcc_match:
        return Compiler(invocation_path, "gcc", int(gcc_match.group(1)), version)
    raise ValueError(f"unsupported compiler identity: {version}")


def validate_compiler_range(compiler: Compiler, system: str, allow: bool) -> None:
    if allow:
        return
    if system == "Darwin":
        valid = compiler.family == "clang" and compiler.major >= 23
        expected = "Darwin Clang >= 23"
    else:
        valid = (
            compiler.family == "gcc" and 11 <= compiler.major <= 16
        ) or (
            compiler.family == "clang" and 17 <= compiler.major <= 23
        )
        expected = "Linux GCC 11..16 or Clang 17..23"
    if not valid:
        raise ValueError(
            f"compiler outside audited range ({expected}): "
            f"{compiler.family} {compiler.major}"
        )


def ensure_tmp_root(path: pathlib.Path) -> None:
    resolved = path.resolve()
    tmp = pathlib.Path("/tmp").resolve()
    if resolved != tmp and tmp not in resolved.parents:
        raise ValueError(f"Darwin build/output root must be below /tmp: {resolved}")


def discover_darwin_sysroot(requested: str | None) -> pathlib.Path:
    if requested:
        sysroot = pathlib.Path(requested).expanduser().resolve()
    else:
        xcrun = pathlib.Path("/usr/bin/xcrun")
        if not xcrun.is_file() or not os.access(xcrun, os.X_OK):
            raise ValueError("/usr/bin/xcrun is unavailable for SDK discovery")
        try:
            result = subprocess.run(
                (str(xcrun), "--show-sdk-path"),
                check=False,
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=10.0,
            )
        except (OSError, subprocess.TimeoutExpired) as error:
            raise ValueError(f"xcrun SDK discovery failed: {error}") from error
        lines = [line.strip() for line in result.stdout.splitlines() if line.strip()]
        if result.returncode != 0 or len(lines) != 1:
            raise ValueError("xcrun --show-sdk-path did not return one SDK path")
        sysroot = pathlib.Path(lines[0]).resolve()
    if not sysroot.is_dir():
        raise ValueError(f"sysroot is unavailable: {sysroot}")
    return sysroot


def linux_cpu_metadata(cpu: int) -> tuple[str, str]:
    affinity = os.sched_getaffinity(0) if hasattr(os, "sched_getaffinity") else set()
    if affinity and cpu not in affinity:
        raise ValueError(f"P-core CPU {cpu} is outside this process affinity: {sorted(affinity)}")
    cpu_root = pathlib.Path(f"/sys/devices/system/cpu/cpu{cpu}")
    if not cpu_root.is_dir():
        raise ValueError(f"P-core CPU does not exist: {cpu}")
    online = cpu_root / "online"
    if online.is_file() and online.read_text(encoding="ascii").strip() == "0":
        raise ValueError(f"P-core CPU is offline: {cpu}")
    core_type_path = cpu_root / "topology" / "core_type"
    siblings_path = cpu_root / "topology" / "thread_siblings_list"
    core_type = (
        core_type_path.read_text(encoding="ascii").strip()
        if core_type_path.is_file()
        else "unreported"
    )
    siblings = (
        siblings_path.read_text(encoding="ascii").strip()
        if siblings_path.is_file()
        else "unreported"
    )
    # Linux does not expose a portable P/E naming convention.  Requiring the
    # explicit option makes the operator's topology audit part of the record;
    # raw kernel metadata is retained so a mistaken selection is detectable.
    return core_type, siblings


def parse_timing(path: pathlib.Path, system: str) -> tuple[Timing, str]:
    try:
        content = path.read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        return Timing(), f"time_metrics_unreadable:{error}"
    if system == "Darwin":
        total_match = next(
            (
                match
                for line in content.splitlines()
                if (match := DARWIN_TIME_TOTAL_PATTERN.fullmatch(line))
            ),
            None,
        )
        rss_match = next(
            (
                match
                for line in content.splitlines()
                if (match := DARWIN_TIME_RSS_PATTERN.fullmatch(line))
            ),
            None,
        )
        if total_match is None or rss_match is None:
            return Timing(), "darwin_time_metrics_missing"
        return Timing(
            total_match.group(1),
            total_match.group(2),
            total_match.group(3),
            rss_match.group(1),
            "0",
        ), ""
    values: dict[str, str] = {}
    for line in content.splitlines():
        match = LINUX_TIME_PATTERN.fullmatch(line)
        if match:
            values[match.group(1)] = match.group(2)
    required = ("wall", "user", "system", "peak_rss_kib", "exit_status")
    if any(key not in values for key in required):
        return Timing(), "gnu_time_metrics_missing"
    try:
        rss_bytes = str(int(values["peak_rss_kib"]) * 1024)
    except ValueError:
        return Timing(), "gnu_time_rss_invalid"
    return Timing(
        values["wall"],
        values["user"],
        values["system"],
        rss_bytes,
        values["exit_status"],
    ), ""


def timed_command(
    command: Sequence[str], timing_path: pathlib.Path, system: str, cpu: int | None
) -> list[str]:
    if system == "Darwin":
        return ["/usr/bin/time", "-l", "-o", str(timing_path), *command]
    assert cpu is not None
    return [
        "/usr/bin/time",
        "-f",
        GNU_TIME_FORMAT,
        "-o",
        str(timing_path),
        "--",
        "/usr/bin/taskset",
        "-c",
        str(cpu),
        *command,
    ]


def record_timing(row: dict[str, object], prefix: str, timing: Timing) -> None:
    row.update(
        {
            f"{prefix}_wall_seconds": timing.wall,
            f"{prefix}_user_seconds": timing.user,
            f"{prefix}_system_seconds": timing.system,
            f"{prefix}_peak_rss_bytes": timing.peak_rss_bytes,
            f"{prefix}_exit_status": timing.exit_status,
        }
    )


def run_timed_pass(
    command: Sequence[str],
    prefix: str,
    directory: pathlib.Path,
    system: str,
    cpu: int | None,
    environment: dict[str, str],
    timeout: float,
) -> tuple[ProcessResult, Timing, str, list[str]]:
    timing_path = directory / f"{prefix}_time.txt"
    wrapped = timed_command(command, timing_path, system, cpu)
    result = run_captured(wrapped, environment, timeout)
    (directory / f"{prefix}_stdout.txt").write_text(
        result.stdout, encoding="utf-8", errors="replace"
    )
    (directory / f"{prefix}_stderr.txt").write_text(
        result.stderr, encoding="utf-8", errors="replace"
    )
    timing, timing_error = parse_timing(timing_path, system)
    timing = dataclasses.replace(timing, exit_status=str(result.returncode))
    if result.timed_out:
        error = f"{prefix}_timeout"
    elif result.returncode != 0:
        error = f"{prefix}_exit_{result.returncode}"
    else:
        error = timing_error
    return result, timing, error, wrapped


def parse_macho_sections(output: str) -> tuple[int, int]:
    text_bytes = 0
    rodata_bytes = 0
    current = ""
    for line in output.splitlines():
        fields = line.strip().split()
        if len(fields) == 2 and fields[0] == "sectname":
            current = fields[1]
        elif len(fields) == 2 and fields[0] == "size" and current:
            try:
                size = int(fields[1], 0)
            except ValueError:
                current = ""
                continue
            if current == "__text":
                text_bytes += size
            elif current in ("__const", "__cstring"):
                rodata_bytes += size
            current = ""
    return text_bytes, rodata_bytes


def parse_elf_sections(output: str) -> tuple[int, int]:
    text_bytes = 0
    rodata_bytes = 0
    for line in output.splitlines():
        fields = line.split()
        if len(fields) < 2:
            continue
        try:
            size = int(fields[1])
        except ValueError:
            continue
        if fields[0] == ".text" or fields[0].startswith(".text."):
            text_bytes += size
        elif fields[0] == ".rodata" or fields[0].startswith(".rodata."):
            rodata_bytes += size
    return text_bytes, rodata_bytes


def measure_image(
    path: pathlib.Path,
    system: str,
    environment: dict[str, str],
    timeout: float,
    prefix: str,
    directory: pathlib.Path,
) -> tuple[dict[str, object], str]:
    if not path.is_file():
        return {}, f"{prefix}_file_missing"
    file_bytes = path.stat().st_size
    if system == "Darwin":
        section_command = ("/usr/bin/otool", "-l", str(path))
        nm_command = ("/usr/bin/nm", "-g", "-U", str(path))
    else:
        section_command = ("/usr/bin/size", "-A", "-d", str(path))
        nm_command = ("/usr/bin/nm", "-g", "--defined-only", str(path))
    section_result = run_captured(section_command, environment, timeout)
    nm_result = run_captured(nm_command, environment, timeout)
    (directory / f"{prefix}_sections.txt").write_text(
        section_result.stdout, encoding="utf-8", errors="replace"
    )
    (directory / f"{prefix}_nm.txt").write_text(
        nm_result.stdout, encoding="utf-8", errors="replace"
    )
    if section_result.timed_out or nm_result.timed_out:
        return {f"{prefix}_file_bytes": file_bytes}, f"{prefix}_image_tool_timeout"
    if section_result.returncode != 0 or nm_result.returncode != 0:
        return {f"{prefix}_file_bytes": file_bytes}, f"{prefix}_image_tool_failed"
    text_bytes, rodata_bytes = (
        parse_macho_sections(section_result.stdout)
        if system == "Darwin"
        else parse_elf_sections(section_result.stdout)
    )
    expected_symbol = "_" + KERNEL_SYMBOL if system == "Darwin" else KERNEL_SYMBOL
    symbol_type = ""
    for line in nm_result.stdout.splitlines():
        fields = line.split()
        if len(fields) >= 2 and fields[-1] == expected_symbol:
            symbol_type = fields[-2]
            break
    values: dict[str, object] = {
        f"{prefix}_file_bytes": file_bytes,
        f"{prefix}_text_bytes": text_bytes,
        f"{prefix}_rodata_bytes": rodata_bytes,
        f"{prefix}_kernel_symbol_type": symbol_type,
    }
    if symbol_type not in ("T", "W", "t", "w"):
        return values, f"{prefix}_kernel_symbol_missing"
    if text_bytes <= 0:
        return values, f"{prefix}_text_section_missing"
    return values, ""


def make_compile_commands(
    compiler: Compiler,
    revision: Revision,
    case: Case,
    source: pathlib.Path,
    object_path: pathlib.Path,
    executable: pathlib.Path,
    standard: str,
    sysroot: pathlib.Path | None,
    fuse_ld: str | None,
    system: str,
    maximum_total_payload: int = DEFAULT_MAXIMUM_TOTAL_PAYLOAD,
) -> tuple[list[str], list[str], list[str], str]:
    common = [str(compiler.path)]
    if compiler.family == "clang":
        common.append("--no-default-config")
    common.extend(
        (
            f"-std={standard}",
            "-DNDEBUG",
            "-fdiagnostics-color=never",
            "-march=native",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-Wconversion",
            "-Wsign-conversion",
            "-Wshadow",
            "-Werror",
        )
    )
    if sysroot is not None:
        common.append(f"--sysroot={sysroot}")
    common.extend(("-I", str(revision.include_root), *case.definitions()))
    common.append(
        f"-DFAST_IO_ORDERED_STAGING_MAXIMUM_TOTAL_PAYLOAD={maximum_total_payload}"
    )
    syntax = [*common, "-fsyntax-only", str(source)]
    object_command = [
        *common,
        "-O3",
        "-ffunction-sections",
        "-fdata-sections",
        "-c",
        str(source),
        "-o",
        str(object_path),
    ]
    link = [str(compiler.path)]
    if compiler.family == "clang":
        link.append("--no-default-config")
    link.extend(("-O3", "-march=native"))
    if sysroot is not None:
        link.append(f"--sysroot={sysroot}")
    if fuse_ld and fuse_ld != "driver-default":
        link.append(f"-fuse-ld={fuse_ld}")
    link.extend(
        (
            str(object_path),
            "-Wl,-dead_strip" if system == "Darwin" else "-Wl,--gc-sections",
            "-o",
            str(executable),
        )
    )
    policy = json.dumps(
        {
            "common": [argument for argument in common if str(revision.include_root) not in argument],
            "object_optimization": "-O3",
            "link_gc": "dead_strip" if system == "Darwin" else "gc-sections",
            "fuse_ld": fuse_ld or "driver-default",
        },
        sort_keys=True,
    )
    return syntax, object_command, link, sha256_bytes(policy.encode("utf-8"))


def blank_build_row(
    run_id: str,
    sequence: int,
    build_id: str,
    case: Case,
    revision: Revision,
    repeat: int,
    order_slot: int,
    compiler: Compiler,
    args: argparse.Namespace,
    system: str,
    source_sha256: str,
    cpu_type: str,
    cpu_siblings: str,
    directory: pathlib.Path,
) -> dict[str, object]:
    row: dict[str, object] = {field: "" for field in BUILD_FIELDS}
    row.update(
        {
            "schema_version": SCHEMA_VERSION,
            "run_id": run_id,
            "sequence": sequence,
            "timestamp_utc": utc_timestamp(),
            "build_id": build_id,
            "group": case.group,
            "case": case.case_id,
            "revision": revision.label,
            "revision_include_sha256": revision.include_sha256,
            "repeat": repeat,
            "order_slot": order_slot,
            "host": platform.node(),
            "platform": system,
            "p_core_cpu": args.p_core_cpu if args.p_core_cpu is not None else "",
            "p_core_type": cpu_type,
            "p_core_thread_siblings": cpu_siblings,
            "compiler_path": str(compiler.path),
            "compiler_family": compiler.family,
            "compiler_major": compiler.major,
            "compiler_version": compiler.version,
            "standard": args.standard,
            "sysroot": str(args.resolved_sysroot) if args.resolved_sysroot else "",
            "march": "native",
            "maximum_total_payload": args.maximum_total_payload,
            "source_sha256": source_sha256,
            "artifact_directory": str(directory),
        }
    )
    return row


def write_row(writer: csv.DictWriter, stream: object, row: dict[str, object]) -> None:
    writer.writerow(row)
    stream.flush()  # type: ignore[attr-defined]


def compile_matrix(
    args: argparse.Namespace,
    cases: Sequence[Case],
    revisions: dict[str, Revision],
    compiler: Compiler,
    environment: dict[str, str],
    frozen_sources: dict[str, bytes],
    source_sha256: str,
    run_root: pathlib.Path,
    run_id: str,
    system: str,
    cpu_type: str,
    cpu_siblings: str,
) -> tuple[list[BuiltArtifact], int]:
    artifacts: list[BuiltArtifact] = []
    failure_count = 0
    build_csv = run_root / "build.csv"
    sequence = 0
    # O-N-N-O is fixed rather than user-configurable: two observations per
    # revision and reversed endpoints expose cold-cache and order asymmetry.
    order = (("old", 1), ("new", 1), ("new", 2), ("old", 2))
    with build_csv.open("x", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=BUILD_FIELDS)
        writer.writeheader()
        stream.flush()
        for case in cases:
            for order_slot, (revision_label, repeat) in enumerate(order, start=1):
                sequence += 1
                revision = revisions[revision_label]
                build_id = f"{run_id}-{sequence:04d}-{revision_label}-r{repeat}"
                directory = run_root / "artifacts" / f"{sequence:04d}-{case.case_id}-{revision_label}-r{repeat}"
                directory.mkdir(parents=True, exist_ok=False)
                for name, content in frozen_sources.items():
                    (directory / name).write_bytes(content)
                source = directory / SOURCE_FILES[0]
                object_path = directory / "case.o"
                executable = directory / "case"
                row = blank_build_row(
                    run_id,
                    sequence,
                    build_id,
                    case,
                    revision,
                    repeat,
                    order_slot,
                    compiler,
                    args,
                    system,
                    source_sha256,
                    cpu_type,
                    cpu_siblings,
                    directory,
                )
                syntax, object_command, link, policy_sha = make_compile_commands(
                    compiler,
                    revision,
                    case,
                    source,
                    object_path,
                    executable,
                    args.standard,
                    args.resolved_sysroot,
                    args.fuse_ld,
                    system,
                    args.maximum_total_payload,
                )
                row["compile_policy_sha256"] = policy_sha
                diagnostics: list[str] = []
                status = "PASS"
                reason = ""
                for prefix, command in (
                    ("syntax", syntax),
                    ("object", object_command),
                    ("link", link),
                ):
                    result, timing, error, wrapped = run_timed_pass(
                        command,
                        prefix,
                        directory,
                        system,
                        args.p_core_cpu,
                        environment,
                        args.compile_timeout_seconds,
                    )
                    row.update(command_fields(prefix, wrapped))
                    record_timing(row, prefix, timing)
                    diagnostics.append(f"[{prefix}] {result.stdout}\n{result.stderr}")
                    if error:
                        status, reason = "FAIL", error
                        break
                    if prefix == "object":
                        values, image_error = measure_image(
                            object_path,
                            system,
                            environment,
                            args.compile_timeout_seconds,
                            "object",
                            directory,
                        )
                        row.update(values)
                        if image_error:
                            status, reason = "FAIL", image_error
                            break
                    elif prefix == "link":
                        values, image_error = measure_image(
                            executable,
                            system,
                            environment,
                            args.compile_timeout_seconds,
                            "linked",
                            directory,
                        )
                        row.update(values)
                        if image_error:
                            status, reason = "FAIL", image_error
                            break
                row.update(
                    status=status,
                    reason=reason,
                    diagnostic_excerpt=normalized_excerpt("\n".join(diagnostics)),
                )
                write_row(writer, stream, row)
                print(
                    f"build {sequence}/{len(cases) * 4} {case.case_id} "
                    f"{revision_label}-r{repeat} {status}",
                    flush=True,
                )
                artifacts.append(
                    BuiltArtifact(
                        build_id,
                        case,
                        revision,
                        repeat,
                        order_slot,
                        directory,
                        executable,
                        status,
                        reason,
                    )
                )
                if status != "PASS":
                    failure_count += 1
    return artifacts, failure_count


def darwin_host_guard(args: argparse.Namespace) -> tuple[str, str, str]:
    load_one = os.getloadavg()[0]
    spotlight_cpu = 0.0
    try:
        result = subprocess.run(
            ("/bin/ps", "-A", "-o", "comm=", "-o", "%cpu="),
            check=False,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=2.0,
        )
        for line in result.stdout.splitlines():
            fields = line.rsplit(maxsplit=1)
            if len(fields) != 2:
                continue
            name = pathlib.Path(fields[0]).name.lower()
            if name.startswith("mdworker") or name in ("mds", "mds_stores", "spotlight"):
                try:
                    spotlight_cpu += float(fields[1])
                except ValueError:
                    pass
    except (OSError, subprocess.TimeoutExpired):
        return f"{load_one:.3f}", "", "darwin_process_activity_unavailable"
    maximum_load = args.max_load_per_cpu * float(os.cpu_count() or 1)
    if not args.allow_busy_host and load_one > maximum_load:
        return f"{load_one:.3f}", f"{spotlight_cpu:.3f}", "host_load_above_limit"
    if not args.allow_busy_host and spotlight_cpu > args.max_spotlight_cpu:
        return f"{load_one:.3f}", f"{spotlight_cpu:.3f}", "spotlight_activity_above_limit"
    return f"{load_one:.3f}", f"{spotlight_cpu:.3f}", ""


def runtime_command(
    artifact: BuiltArtifact,
    profile: str,
    args: argparse.Namespace,
    system: str,
) -> list[str]:
    command = [
        str(artifact.executable),
        profile,
        str(args.seed),
        str(args.target_milliseconds),
    ]
    if system == "Linux":
        assert args.p_core_cpu is not None
        command = ["/usr/bin/taskset", "-c", str(args.p_core_cpu), *command]
    return command


def parse_runtime(
    text: str, artifact: BuiltArtifact, profile: str, seed: int
) -> tuple[dict[str, str], str]:
    lines = [line.strip() for line in text.splitlines() if line.strip()]
    if len(lines) != 1:
        return {}, "runtime_output_line_count"
    fields = lines[0].split(",")
    if len(fields) != len(RUNTIME_HEADER):
        return {}, "runtime_output_field_count"
    values = dict(zip(RUNTIME_HEADER, fields, strict=True))
    case = artifact.case
    expected_total = case.pack * 23 if profile == "small" else int(profile)
    expected = {
        "operation": "concat-ordered-staging",
        "source": case.source_name,
        "topology": case.topology_name,
        "pack": str(case.pack),
        "line": str(case.line),
        "result": case.result_name,
        "total": str(expected_total),
        "seed": str(seed),
    }
    for key, expected_value in expected.items():
        if values[key] != expected_value:
            return values, f"runtime_identity_mismatch:{key}"
    try:
        if int(values["iterations"]) <= 0:
            raise ValueError("nonpositive iterations")
        seconds = float(values["seconds"])
        nanoseconds = float(values["ns_per_call"])
        int(values["checksum"])
        int(values["validation_digest"])
        if not (0.0 < seconds < 0.8) or nanoseconds <= 0.0:
            raise ValueError("runtime measurement outside admitted range")
    except ValueError as error:
        return values, f"runtime_metric_invalid:{error}"
    return values, ""


def blank_runtime_row(
    run_id: str,
    sequence: int,
    artifact: BuiltArtifact,
    profile: str,
    runtime_order_value: int,
    args: argparse.Namespace,
    system: str,
    load_one: str,
    spotlight_cpu: str,
) -> dict[str, object]:
    row: dict[str, object] = {field: "" for field in RUNTIME_FIELDS}
    row.update(
        {
            "schema_version": SCHEMA_VERSION,
            "run_id": run_id,
            "sequence": sequence,
            "timestamp_utc": utc_timestamp(),
            "build_id": artifact.build_id,
            "group": artifact.case.group,
            "case": artifact.case.case_id,
            "revision": artifact.revision.label,
            "repeat": artifact.repeat,
            "order_slot": artifact.order_slot,
            "profile": profile,
            "maximum_total_payload": args.maximum_total_payload,
            "runtime_order": runtime_order_value,
            "host": platform.node(),
            "platform": system,
            "p_core_cpu": args.p_core_cpu if args.p_core_cpu is not None else "",
            "host_load_one": load_one,
            "spotlight_cpu_percent": spotlight_cpu,
            "target_milliseconds": args.target_milliseconds,
            "artifact_directory": str(artifact.directory),
        }
    )
    return row


def run_matrix(
    args: argparse.Namespace,
    artifacts: Sequence[BuiltArtifact],
    environment: dict[str, str],
    run_root: pathlib.Path,
    run_id: str,
    system: str,
) -> int:
    runtime_csv = run_root / "runtime.csv"
    sequence = 0
    failures = 0
    by_case: dict[str, list[BuiltArtifact]] = {}
    for artifact in artifacts:
        by_case.setdefault(artifact.case.case_id, []).append(artifact)
    with runtime_csv.open("x", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=RUNTIME_FIELDS)
        writer.writeheader()
        stream.flush()
        for case_id in dict.fromkeys(artifact.case.case_id for artifact in artifacts):
            case_artifacts = by_case[case_id]
            for profile in args.profiles:
                load_one, spotlight_cpu, guard_error = (
                    darwin_host_guard(args) if system == "Darwin" else ("", "", "")
                )
                pending: list[dict[str, object]] = []
                for runtime_order_value, artifact in enumerate(case_artifacts, start=1):
                    sequence += 1
                    row = blank_runtime_row(
                        run_id,
                        sequence,
                        artifact,
                        profile,
                        runtime_order_value,
                        args,
                        system,
                        load_one,
                        spotlight_cpu,
                    )
                    if artifact.status != "PASS":
                        row.update(status="SKIP", reason=f"build_{artifact.reason}")
                    elif guard_error:
                        row.update(status="SKIP", reason=guard_error)
                    else:
                        command = runtime_command(artifact, profile, args, system)
                        row.update(command_fields("runtime", command))
                        result = run_captured(
                            command, environment, args.runtime_timeout_seconds
                        )
                        (artifact.directory / f"runtime_{profile}_stdout.txt").write_text(
                            result.stdout, encoding="utf-8", errors="replace"
                        )
                        (artifact.directory / f"runtime_{profile}_stderr.txt").write_text(
                            result.stderr, encoding="utf-8", errors="replace"
                        )
                        values, parse_error = parse_runtime(
                            result.stdout, artifact, profile, args.seed
                        )
                        row.update(values)
                        row["stdout_sha256"] = sha256_bytes(result.stdout.encode("utf-8"))
                        row["diagnostic_excerpt"] = normalized_excerpt(result.stderr)
                        if result.timed_out:
                            row.update(status="FAIL", reason="runtime_timeout")
                        elif result.returncode != 0:
                            row.update(
                                status="FAIL",
                                reason=f"runtime_exit_{result.returncode}",
                            )
                        elif parse_error:
                            row.update(status="FAIL", reason=parse_error)
                        else:
                            row.update(status="PASS", reason="")
                    pending.append(row)
                digests = {
                    str(row["validation_digest"])
                    for row in pending
                    if row["status"] == "PASS"
                }
                if len(digests) > 1:
                    for row in pending:
                        if row["status"] == "PASS":
                            row.update(status="FAIL", reason="old_new_digest_mismatch")
                for row in pending:
                    write_row(writer, stream, row)
                    if row["status"] != "PASS":
                        failures += 1
                profile_status = (
                    "PASS" if all(row["status"] == "PASS" for row in pending) else "INCOMPLETE"
                )
                print(
                    f"runtime {case_id} profile={profile} {profile_status}",
                    flush=True,
                )
    return failures


def select_cases(args: argparse.Namespace) -> tuple[Case, ...]:
    cases = matrix_cases()
    if args.group:
        selected_groups = set(args.group)
        cases = tuple(case for case in cases if case.group in selected_groups)
    if args.case:
        selected_ids = set(args.case)
        unknown = selected_ids.difference(case.case_id for case in matrix_cases())
        if unknown:
            raise ValueError("unknown case(s): " + ", ".join(sorted(unknown)))
        cases = tuple(case for case in cases if case.case_id in selected_ids)
    if not cases:
        raise ValueError("case selection is empty")
    return cases


def parse_arguments(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--new-root", default=str(REPOSITORY_ROOT))
    parser.add_argument("--old-root", default=str(DEFAULT_OLD_ROOT))
    parser.add_argument("--compiler")
    parser.add_argument("--sysroot")
    parser.add_argument("--standard", choices=("c++20", "c++23", "c++2b"), default="c++23")
    parser.add_argument("--fuse-ld")
    parser.add_argument("--output-root")
    parser.add_argument("--p-core-cpu", type=int)
    parser.add_argument("--target-milliseconds", type=int, default=40)
    parser.add_argument("--seed", type=int, default=7640891576956012809)
    parser.add_argument(
        "--profiles",
        nargs="+",
        choices=PROFILE_CHOICES,
        default=list(DEFAULT_PROFILES),
        help=(
            "payload profiles; opt-in larger profiles compile an independent fixture "
            "whose static reserve bound is max(2049, selected payloads); the default "
            "small/2047/2048/2049 regression is unchanged"
        ),
    )
    parser.add_argument(
        "--group",
        action="append",
        choices=("main", "supplemental-line", "supplemental-topology"),
    )
    parser.add_argument("--case", action="append")
    parser.add_argument("--compile-timeout-seconds", type=float, default=120.0)
    parser.add_argument("--runtime-timeout-seconds", type=float, default=1.0)
    parser.add_argument("--cooldown-seconds", type=float, default=2.0)
    parser.add_argument("--max-load-per-cpu", type=float, default=0.5)
    parser.add_argument("--max-spotlight-cpu", type=float, default=5.0)
    parser.add_argument("--allow-busy-host", action="store_true")
    parser.add_argument("--allow-compiler-outside-audit-range", action="store_true")
    parser.add_argument("--list-cases", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)
    if not 20 <= args.target_milliseconds <= 80:
        parser.error("--target-milliseconds must be in 20..80")
    if args.seed < 0 or args.seed > (1 << 64) - 1:
        parser.error("--seed must fit uint64_t")
    if args.compile_timeout_seconds <= 0.0 or not 0.8 <= args.runtime_timeout_seconds <= 1.2:
        parser.error("timeouts must be positive and runtime timeout must be in 0.8..1.2")
    if args.cooldown_seconds < 0.0 or args.cooldown_seconds > 30.0:
        parser.error("--cooldown-seconds must be in 0..30")
    if args.max_load_per_cpu <= 0.0 or args.max_spotlight_cpu < 0.0:
        parser.error("host activity limits must be nonnegative")
    args.maximum_total_payload = max(
        (
            DEFAULT_MAXIMUM_TOTAL_PAYLOAD,
            *(int(profile) for profile in args.profiles if profile != "small"),
        )
    )
    return args


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_arguments(sys.argv[1:] if argv is None else argv)
    try:
        cases = select_cases(args)
    except ValueError as error:
        print(error, file=sys.stderr)
        return 2
    if args.list_cases:
        for case in cases:
            print(f"{case.group},{case.case_id}")
        return 0
    system = platform.system()
    if system not in ("Darwin", "Linux"):
        print("execution is supported only on Darwin and Linux", file=sys.stderr)
        return 2
    if system == "Darwin":
        compiler_path = pathlib.Path(args.compiler) if args.compiler else DEFAULT_DARWIN_CLANG
        try:
            sysroot = discover_darwin_sysroot(args.sysroot)
        except ValueError as error:
            print(error, file=sys.stderr)
            return 2
        args.resolved_sysroot = sysroot
        if args.fuse_ld is None:
            # The custom LLVM 23 driver requires its matching ld64.lld here;
            # the installed Apple linker cannot consume that toolchain's
            # builtins bitcode when it is selected implicitly.
            args.fuse_ld = "lld"
        if args.p_core_cpu is not None:
            print("--p-core-cpu is Linux-only", file=sys.stderr)
            return 2
    else:
        compiler_name = args.compiler or "clang++"
        resolved_name = shutil.which(compiler_name)
        if resolved_name is None:
            print(f"compiler is unavailable: {compiler_name}", file=sys.stderr)
            return 2
        compiler_path = pathlib.Path(resolved_name)
        args.resolved_sysroot = pathlib.Path(args.sysroot).resolve() if args.sysroot else None
        if args.p_core_cpu is None:
            print("Linux execution requires --p-core-cpu with a verified P-core", file=sys.stderr)
            return 2
    if args.output_root:
        run_root = pathlib.Path(args.output_root).expanduser().resolve()
        run_root.mkdir(parents=True, exist_ok=False)
    else:
        run_root = pathlib.Path(
            tempfile.mkdtemp(prefix="fast_io_concat_ordered_staging.", dir="/tmp")
        )
    try:
        if system == "Darwin":
            ensure_tmp_root(run_root)
        cpu_type, cpu_siblings = (
            linux_cpu_metadata(args.p_core_cpu)
            if system == "Linux"
            else ("apple-performance-core-scheduler", "not-applicable")
        )
        frozen_sources, source_sha256 = freeze_sources()
        revisions = {
            "old": resolve_revision("old", args.old_root),
            "new": resolve_revision("new", args.new_root),
        }
        provisional_environment = sanitized_environment(run_root, compiler_path.absolute())
        compiler = probe_compiler(compiler_path, provisional_environment)
        validate_compiler_range(
            compiler, system, args.allow_compiler_outside_audit_range
        )
        environment = sanitized_environment(run_root, compiler.path)
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        return 2
    run_id = uuid.uuid4().hex
    manifest = {
        "schema_version": SCHEMA_VERSION,
        "run_id": run_id,
        "created_utc": utc_timestamp(),
        "platform": system,
        "host": platform.node(),
        "matrix_counts": {
            "selected": len(cases),
            "main": sum(case.group == "main" for case in cases),
            "supplemental": sum(case.group != "main" for case in cases),
        },
        "cases": [dataclasses.asdict(case) for case in cases],
        "profiles": args.profiles,
        "maximum_total_payload": args.maximum_total_payload,
        "build_order": ["old-r1", "new-r1", "new-r2", "old-r2"],
        "source_sha256": source_sha256,
        "compiler": dataclasses.asdict(compiler) | {"path": str(compiler.path)},
        "revisions": {
            label: {
                "root": str(revision.root),
                "include_sha256": revision.include_sha256,
            }
            for label, revision in revisions.items()
        },
        "p_core_cpu": args.p_core_cpu,
        "p_core_type": cpu_type,
        "p_core_thread_siblings": cpu_siblings,
        "target_milliseconds": args.target_milliseconds,
        "seed": args.seed,
        "output_root": str(run_root),
    }
    (run_root / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    if args.dry_run:
        print(json.dumps(manifest, indent=2, sort_keys=True))
        return 0
    artifacts, build_failures = compile_matrix(
        args,
        cases,
        revisions,
        compiler,
        environment,
        frozen_sources,
        source_sha256,
        run_root,
        run_id,
        system,
        cpu_type,
        cpu_siblings,
    )
    if args.cooldown_seconds:
        time.sleep(args.cooldown_seconds)
    runtime_failures = run_matrix(
        args, artifacts, environment, run_root, run_id, system
    )
    print(
        f"artifacts={run_root} build_failures={build_failures} "
        f"runtime_failures={runtime_failures}"
    )
    return 1 if build_failures or runtime_failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
