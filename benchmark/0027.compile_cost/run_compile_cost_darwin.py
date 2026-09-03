#!/usr/bin/env python3
"""Measure isolated fast_io compilation cost with one Darwin Clang process at a time.

The generated cases and optional extension-slot protocol are imported from the
Linux runner.  Execution is deliberately Darwin-only, while ``--list-cases``
and ``--dry-run`` are safe on every host and never invoke a compiler.
"""

from __future__ import annotations

import argparse
import csv
import dataclasses
import json
import os
import pathlib
import platform
import re
import sys
import tempfile
import uuid
from collections.abc import Sequence


SCRIPT_DIRECTORY = pathlib.Path(__file__).resolve().parent
sys.dont_write_bytecode = True
if str(SCRIPT_DIRECTORY) not in sys.path:
    # Importing the sibling module keeps the case bodies and extension-selector
    # contract byte-identical across Linux and Darwin instead of copying them.
    # Bytecode is disabled so even runner-internal artifacts stay out of the
    # shared repository and the /tmp-only materialization contract remains true.
    sys.path.insert(0, str(SCRIPT_DIRECTORY))
import run_compile_cost as common  # noqa: E402


SCHEMA_VERSION = "darwin-2"
DEFAULT_CLANG = pathlib.Path(
    "/Users/liyinan/Documents/MacroModel/tool-chain/tools/"
    "aarch64-apple-darwin-llvm/llvm/bin/clang++"
)
DEFAULT_SYSROOT = pathlib.Path(
    "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk"
)
DARWIN_TIME = pathlib.Path("/usr/bin/time")
DARWIN_OTOOL = pathlib.Path("/usr/bin/otool")
DARWIN_NM = pathlib.Path("/usr/bin/nm")
DEFAULT_TIMEOUT_SECONDS = 120.0
PROBE_TIMEOUT_SECONDS = 10.0
FLOAT_PATTERN = r"(?:\d+(?:\.\d*)?|\.\d+)"
TIME_TOTAL_PATTERN = re.compile(
    rf"^\s*({FLOAT_PATTERN})\s+real\s+({FLOAT_PATTERN})\s+user\s+"
    rf"({FLOAT_PATTERN})\s+sys\s*$"
)
TIME_RSS_PATTERN = re.compile(r"^\s*(\d+)\s+maximum resident set size\s*$")
NM_RECORD_PATTERN = re.compile(r"^(?:[0-9A-Fa-f]+\s+)?([A-Za-z?])\s+(\S+)\s*$")
FIXED_PATH = "/usr/bin:/bin:/usr/sbin:/sbin"


CSV_FIELDS = (
    "schema_version",
    "run_id",
    "sequence",
    "timestamp_utc",
    "host",
    "revision",
    "include_root",
    "compiler",
    "compiler_requested_path",
    "compiler_path",
    "compiler_family",
    "compiler_major",
    "compiler_version",
    "standard",
    "sysroot",
    "march",
    "fuse_ld",
    "case",
    "case_family",
    "pack_size",
    "repeat",
    "status",
    "reason",
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
    "object_bytes",
    "macho_text_bytes",
    "macho_const_bytes",
    "macho_cstring_bytes",
    "defined_external_symbol_count",
    "entry_point_symbol",
    "entry_point_macho_symbol",
    "entry_point_symbol_type",
    "environment_policy_sha256",
    "environment_policy_json",
    "source_sha256",
    "syntax_command_sha256",
    "syntax_comparable_command_sha256",
    "syntax_command_json",
    "object_command_sha256",
    "object_comparable_command_sha256",
    "object_command_json",
    "diagnostic_excerpt",
    "artifact_directory",
)


@dataclasses.dataclass(frozen=True)
class CompilerProbe:
    path: pathlib.Path | None
    version: str
    major: int | None
    error: str


@dataclasses.dataclass(frozen=True)
class DarwinTools:
    time: pathlib.Path | None
    otool: pathlib.Path | None
    nm: pathlib.Path | None
    error: str


@dataclasses.dataclass(frozen=True)
class SourceMaterial:
    content: bytes
    origin: pathlib.Path | None
    entry_point_symbol: str
    origin_identity: tuple[int, int, int, int, int, int] | None = None


@dataclasses.dataclass(frozen=True)
class SourceResolution:
    material: SourceMaterial | None
    status: str
    reason: str


def resolve_fixed_tool(path: pathlib.Path) -> pathlib.Path | None:
    resolved = path.resolve(strict=False)
    if resolved.is_file() and os.access(resolved, os.X_OK):
        return resolved
    return None


def resolve_tools() -> DarwinTools:
    time_path = resolve_fixed_tool(DARWIN_TIME)
    otool_path = resolve_fixed_tool(DARWIN_OTOOL)
    nm_path = resolve_fixed_tool(DARWIN_NM)
    errors: list[str] = []
    if time_path is None:
        errors.append("darwin_time_unavailable")
    if otool_path is None:
        errors.append("otool_unavailable")
    if nm_path is None:
        errors.append("nm_unavailable")
    return DarwinTools(time_path, otool_path, nm_path, ";".join(errors))


def probe_compiler(requested: pathlib.Path, environment: dict[str, str]) -> CompilerProbe:
    """Verify that the single explicitly named driver is Clang, not a PATH alias."""

    resolved = common.resolve_program(str(requested))
    if resolved is None:
        return CompilerProbe(None, "", None, "explicit_compiler_unavailable")
    try:
        process = common.run_captured(
            (str(resolved), "--no-default-config", "--version"),
            timeout=PROBE_TIMEOUT_SECONDS,
            environment=environment,
        )
    except OSError as error:
        return CompilerProbe(resolved, "", None, f"compiler_probe_os_error:{error}")
    if process.timed_out:
        return CompilerProbe(resolved, "", None, "compiler_probe_timeout")
    if process.returncode != 0:
        return CompilerProbe(resolved, "", None, "compiler_probe_nonzero_exit")
    lines = [
        line.strip()
        for line in (process.stdout + process.stderr).splitlines()
        if line.strip()
    ]
    version = lines[0] if lines else ""
    match = re.search(r"(?:Apple\s+)?clang version\s+(\d+)", version, re.IGNORECASE)
    if match is None:
        return CompilerProbe(
            resolved, version, None, "compiler_family_mismatch:expected=clang"
        )
    return CompilerProbe(resolved, version, int(match.group(1)), "")


def validate_experiment_flags(args: argparse.Namespace) -> None:
    """Reject flags capable of silently changing a fixed comparison invariant."""

    forbidden_exact = {
        "-c",
        "-fsyntax-only",
        "-E",
        "-S",
        "-o",
        "-x",
        "-std",
        "--sysroot",
        "-isysroot",
        "-arch",
        "-target",
        "--target",
        "-Xclang",
        "-Xassembler",
        "-emit-llvm",
        "--config",
        "--config-system-dir",
        "--config-user-dir",
        "-resource-dir",
        "--gcc-toolchain",
        "-gcc-toolchain",
        "--gcc-install-dir",
        "--offload-arch",
        "--cuda-gpu-arch",
        "-fopenmp-targets",
        "-fmodules-cache-path",
        "-fprebuilt-module-path",
        "-fmodule-file",
    }
    forbidden_prefixes = (
        "-std=",
        "-x",
        "--sysroot=",
        "-isysroot",
        # Reject the complete optimization and machine-option namespaces.  This
        # covers current and future spellings such as -Og, -Ofast, -O4, -m64,
        # -mcpu, -mtune and backend forwarding through -mllvm.
        "-O",
        "-m",
        "-arch",
        "-target",
        "--target=",
        "-ccc-",
        "-darwin-target-variant",
        "-Xclang",
        "-Xassembler",
        "-Xarch_",
        "-Xopenmp-target",
        "-Xoffload-",
        "-Xcuda-",
        "-Wa,",
        "--config=",
        "--config-system-dir=",
        "--config-user-dir=",
        "-resource-dir=",
        "--gcc-toolchain=",
        "-gcc-toolchain=",
        "--gcc-install-dir=",
        "--offload-arch=",
        "--cuda-gpu-arch=",
        "-fopenmp-targets=",
        "-B",
        "-o",
        "-ffunction-sections",
        "-fno-function-sections",
        "-fdata-sections",
        "-fno-data-sections",
        "-fmodules-cache-path",
        "-fprebuilt-module-path",
        "-fmodule-file",
        "-flto",
        "-fno-lto",
    )
    for option_name, values in (
        ("--cxxflag", args.cxxflag),
        ("--syntax-flag", args.syntax_flag),
        ("--object-flag", args.object_flag),
    ):
        for value in values:
            # A response file can contain any otherwise forbidden option, so it
            # is not an auditable extra flag and is rejected as an escape hatch.
            if (
                value in forbidden_exact
                or value.startswith(forbidden_prefixes)
                or value.startswith("@")
            ):
                raise ValueError(
                    f"{option_name}={value!r} overrides a fixed experiment invariant"
                )


def make_sanitized_environment(
    work_root: pathlib.Path,
    *,
    materialize: bool = True,
) -> tuple[dict[str, str], str, str]:
    """Build and record a closed compiler environment instead of inheriting it."""

    home = work_root / "environment_home"
    temporary = work_root / "environment_tmp"
    xdg_cache = work_root / "xdg_cache"
    xdg_config = work_root / "xdg_config"
    if materialize:
        for directory in (home, temporary, xdg_cache, xdg_config):
            directory.mkdir(parents=True, exist_ok=False)
    environment = {
        "HOME": str(home),
        "LANG": "C",
        "LC_ALL": "C",
        "PATH": FIXED_PATH,
        "TMPDIR": str(temporary),
        "XDG_CACHE_HOME": str(xdg_cache),
        "XDG_CONFIG_HOME": str(xdg_config),
    }
    policy = {
        "default_clang_config": "disabled_by_--no-default-config",
        "environment": environment,
        "inheritance": "none",
        "module_cache": "unique_per_pass_via_-fmodules-cache-path",
    }
    policy_json = json.dumps(policy, ensure_ascii=False, sort_keys=True)
    return environment, policy_json, common.sha256_bytes(policy_json.encode("utf-8"))


def normalize_sysroot(value: str) -> tuple[pathlib.Path, str]:
    path = pathlib.Path(value).expanduser().resolve(strict=False)
    return path, "" if path.is_dir() else f"sysroot_missing:{path}"


def normalize_requested_compiler(value: str) -> pathlib.Path:
    path = pathlib.Path(value).expanduser()
    if not path.is_absolute():
        raise ValueError("--compiler must be an explicit absolute path")
    # Preserve the user-selected symlink spelling in the CSV; CompilerProbe.path
    # separately records the fully resolved executable used in exact argv.
    return path


def strict_revision_order(
    revisions: Sequence[common.Revision],
) -> tuple[tuple[common.Revision, int], ...]:
    """Return the mandatory ABBA schedule; no caller can weaken its pairing."""

    by_label = {revision.label: revision for revision in revisions}
    if len(revisions) != 2 or set(by_label) != {"old", "new"}:
        raise ValueError(
            "Darwin execution requires exactly --include-root old=PATH and new=PATH"
        )
    return (
        (by_label["old"], 1),
        (by_label["new"], 1),
        (by_label["new"], 2),
        (by_label["old"], 2),
    )


def file_identity(status: os.stat_result) -> tuple[int, int, int, int, int, int]:
    """Capture metadata sufficient to reject ordinary replacement or mutation."""

    return (
        status.st_dev,
        status.st_ino,
        status.st_mode,
        status.st_size,
        status.st_mtime_ns,
        status.st_ctime_ns,
    )


def read_frozen_extension(
    path: pathlib.Path,
) -> tuple[
    bytes | None,
    tuple[int, int, int, int, int, int] | None,
    str,
]:
    """Read an extension exactly once and reject a concurrent freeze-time write."""

    try:
        with path.open("rb") as source_stream:
            before = file_identity(os.fstat(source_stream.fileno()))
            content = source_stream.read()
            after = file_identity(os.fstat(source_stream.fileno()))
        path_after = file_identity(path.stat())
    except OSError as error:
        return None, None, f"extension_source_unreadable:{error}"
    if before != after or after != path_after:
        return None, None, f"extension_source_mutated_during_freeze:{path}"
    return content, after, ""


def freeze_source_materials(
    cases: Sequence[common.CompileCase],
    revisions: Sequence[common.Revision],
    extensions: dict[tuple[str, str | None], pathlib.Path],
    extension_symbols: dict[tuple[str, str | None], str],
) -> dict[tuple[str, str], SourceResolution]:
    """Freeze every selected source before any member of its ABBA schedule runs."""

    result: dict[tuple[str, str], SourceResolution] = {}
    builtins: dict[str, SourceMaterial] = {}
    extension_files: dict[
        pathlib.Path,
        tuple[bytes | None, tuple[int, int, int, int, int, int] | None, str],
    ] = {}
    for case in cases:
        if case.extension_slot is None:
            assert case.source_factory is not None
            assert case.builtin_oracle_symbol is not None
            builtins[case.case_id] = SourceMaterial(
                case.source_factory().encode("utf-8"),
                None,
                case.builtin_oracle_symbol,
            )
        for revision in revisions:
            key = (case.case_id, revision.label)
            if case.extension_slot is None:
                result[key] = SourceResolution(builtins[case.case_id], "", "")
                continue
            source = common.resolve_extension_source(case, revision, extensions)
            if source is None:
                result[key] = SourceResolution(
                    None, "SKIP", "optional_extension_source_not_supplied"
                )
                continue
            if not source.is_file():
                result[key] = SourceResolution(
                    None, "FAIL", f"declared_extension_source_missing:{source}"
                )
                continue
            symbol = common.resolve_extension_symbol(
                case, revision, extension_symbols
            )
            if symbol is None:
                result[key] = SourceResolution(
                    None, "FAIL", "extension_entry_point_symbol_not_supplied"
                )
                continue
            if source not in extension_files:
                extension_files[source] = read_frozen_extension(source)
            content, identity, error = extension_files[source]
            if error:
                result[key] = SourceResolution(None, "FAIL", error)
                continue
            assert content is not None and identity is not None
            result[key] = SourceResolution(
                SourceMaterial(content, source, symbol, identity), "", ""
            )
    return result


def verify_frozen_source(material: SourceMaterial) -> str:
    """Fail on post-freeze mutation without replacing the immutable source bytes."""

    if material.origin is None:
        return ""
    assert material.origin_identity is not None
    try:
        current = file_identity(material.origin.stat())
    except OSError as error:
        return f"extension_source_changed_after_freeze:{error}"
    if current != material.origin_identity:
        return f"extension_source_changed_after_freeze:{material.origin}"
    return ""


def append_common_compile_options(
    command: list[str],
    args: argparse.Namespace,
    compiler_path: pathlib.Path,
    include_root: pathlib.Path,
    extension_origin: pathlib.Path | None,
    sysroot: pathlib.Path,
    module_cache: pathlib.Path,
) -> None:
    command.extend(
        (
            str(compiler_path),
            "--no-default-config",
            f"-std={args.standard}",
            "-DNDEBUG",
            "-fdiagnostics-color=never",
            f"--sysroot={sysroot}",
            "-march=native",
            f"-fmodules-cache-path={module_cache}",
        )
    )
    if args.fuse_ld:
        # This records the requested driver policy consistently.  Both measured
        # passes stop before linking, so linker startup is never timed.
        command.append(f"-fuse-ld={args.fuse_ld}")
    command.extend(("-I", str(include_root)))
    if extension_origin is not None:
        command.extend(("-iquote", str(extension_origin.parent)))


def build_syntax_command(
    args: argparse.Namespace,
    compiler_path: pathlib.Path,
    include_root: pathlib.Path,
    source_path: pathlib.Path,
    timing_path: pathlib.Path,
    extension_origin: pathlib.Path | None,
    sysroot: pathlib.Path,
    module_cache: pathlib.Path,
) -> list[str]:
    command = [str(DARWIN_TIME), "-l", "-o", str(timing_path)]
    append_common_compile_options(
        command,
        args,
        compiler_path,
        include_root,
        extension_origin,
        sysroot,
        module_cache,
    )
    command.extend(args.cxxflag)
    command.extend(args.syntax_flag)
    command.extend(("-fsyntax-only", str(source_path)))
    return command


def build_object_command(
    args: argparse.Namespace,
    compiler_path: pathlib.Path,
    include_root: pathlib.Path,
    source_path: pathlib.Path,
    object_path: pathlib.Path,
    timing_path: pathlib.Path,
    extension_origin: pathlib.Path | None,
    sysroot: pathlib.Path,
    module_cache: pathlib.Path,
) -> list[str]:
    command = [str(DARWIN_TIME), "-l", "-o", str(timing_path)]
    append_common_compile_options(
        command,
        args,
        compiler_path,
        include_root,
        extension_origin,
        sysroot,
        module_cache,
    )
    command.extend(("-O2", "-ffunction-sections", "-fdata-sections"))
    command.extend(args.cxxflag)
    command.extend(args.object_flag)
    command.extend(("-c", str(source_path), "-o", str(object_path)))
    return command


def command_fields(
    prefix: str,
    command: Sequence[str],
    comparison_replacements: dict[str, str],
) -> dict[str, str]:
    command_json = json.dumps(list(command), ensure_ascii=False)
    normalized: list[str] = []
    replacements = sorted(
        comparison_replacements.items(), key=lambda item: len(item[0]), reverse=True
    )

    def replace_path(value: str) -> str:
        for concrete, placeholder in replacements:
            if value == concrete:
                return placeholder
            if value.startswith(concrete + os.sep):
                return placeholder + value[len(concrete) :]
        return value

    for argument in command:
        option, separator, value = argument.partition("=")
        comparable_argument = (
            option + separator + replace_path(value)
            if separator
            else replace_path(argument)
        )
        normalized.append(comparable_argument)
    comparable_json = json.dumps(normalized, ensure_ascii=False)
    return {
        f"{prefix}_command_json": command_json,
        f"{prefix}_command_sha256": common.sha256_bytes(command_json.encode("utf-8")),
        f"{prefix}_comparable_command_sha256": common.sha256_bytes(
            comparable_json.encode("utf-8")
        ),
    }


def parse_darwin_time(path: pathlib.Path) -> tuple[dict[str, str], str]:
    """Parse BSD time(1); Darwin reports maximum resident set size in bytes."""

    try:
        content = path.read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        return {}, f"time_metrics_unreadable:{error}"
    values: dict[str, str] = {}
    for line in content.splitlines():
        total_match = TIME_TOTAL_PATTERN.fullmatch(line)
        if total_match:
            values.update(
                wall=total_match.group(1),
                user=total_match.group(2),
                system=total_match.group(3),
            )
        rss_match = TIME_RSS_PATTERN.fullmatch(line)
        if rss_match:
            values["peak_rss_bytes"] = rss_match.group(1)
    missing = [
        key for key in ("wall", "user", "system", "peak_rss_bytes") if key not in values
    ]
    if missing:
        return values, "time_metrics_missing:" + ",".join(missing)
    try:
        if any(float(values[key]) < 0.0 for key in ("wall", "user", "system")):
            raise ValueError("negative elapsed metric")
        if int(values["peak_rss_bytes"]) < 0:
            raise ValueError("negative RSS")
    except ValueError as error:
        return values, f"time_metrics_invalid:{error}"
    return values, ""


def record_time_metrics(
    row: dict[str, object],
    prefix: str,
    values: dict[str, str],
    returncode: int,
) -> None:
    row.update(
        {
            f"{prefix}_wall_seconds": values.get("wall", ""),
            f"{prefix}_user_seconds": values.get("user", ""),
            f"{prefix}_system_seconds": values.get("system", ""),
            f"{prefix}_peak_rss_bytes": values.get("peak_rss_bytes", ""),
            f"{prefix}_exit_status": returncode,
        }
    )


def compiler_pass_error(
    prefix: str,
    process: common.CapturedProcess,
    timing_error: str,
) -> str:
    if process.timed_out:
        return f"{prefix}_timeout"
    if process.returncode != 0:
        return f"{prefix}_exit_{process.returncode}"
    if timing_error:
        return f"{prefix}_{timing_error}"
    return ""


def parse_macho_sections(output: str) -> tuple[dict[str, int], str]:
    totals = {"__text": 0, "__const": 0, "__cstring": 0}
    observed: dict[str, int] = {}
    consumed: dict[str, int] = {}
    current_section: str | None = None
    for line in output.splitlines():
        fields = line.strip().split()
        if len(fields) == 2 and fields[0] == "sectname":
            current_section = fields[1]
            if current_section in totals:
                observed[current_section] = observed.get(current_section, 0) + 1
            continue
        if current_section is not None and len(fields) == 2 and fields[0] == "size":
            if current_section in totals:
                try:
                    totals[current_section] += int(fields[1], 0)
                except ValueError:
                    return (
                        totals,
                        f"otool_section_size_invalid:{current_section}:{fields[1]}",
                    )
                consumed[current_section] = consumed.get(current_section, 0) + 1
            current_section = None
    if "__text" not in observed:
        return totals, "otool_text_section_missing"
    for section, count in observed.items():
        if consumed.get(section, 0) != count:
            return totals, f"otool_section_size_missing:{section}"
    return totals, ""


def parse_nm_entry_point(
    output: str, required_symbol: str
) -> tuple[dict[str, int | str], str]:
    expected_macho_symbol = "_" + required_symbol
    count = 0
    entry_point_type = ""
    for line in output.splitlines():
        match = NM_RECORD_PATTERN.fullmatch(line.strip())
        if match is None:
            continue
        symbol_type, symbol = match.groups()
        count += 1
        if symbol == expected_macho_symbol:
            entry_point_type = symbol_type
    values: dict[str, int | str] = {
        "defined_external_symbol_count": count,
        "entry_point_macho_symbol": expected_macho_symbol,
    }
    if not entry_point_type:
        return values, f"entry_point_symbol_missing:{required_symbol}"
    values["entry_point_symbol_type"] = entry_point_type
    # This proves only that the named external wrapper remains a text entry
    # point.  Retention of operations inside it requires a separate dependency
    # oracle or disassembly audit and is deliberately not inferred from nm.
    if entry_point_type not in ("T", "W"):
        return values, (
            f"entry_point_symbol_not_external_text:{required_symbol}:{entry_point_type}"
        )
    return values, ""


def measure_object(
    object_path: pathlib.Path,
    required_symbol: str,
    tools: DarwinTools,
    environment: dict[str, str],
    timeout: float,
    artifact_directory: pathlib.Path,
) -> tuple[dict[str, int | str], str]:
    if not object_path.is_file():
        return {}, "object_not_produced"
    try:
        object_bytes = object_path.stat().st_size
    except OSError as error:
        return {}, f"object_stat_failed:{error}"
    if object_bytes <= 0:
        return {}, "object_is_empty"
    assert tools.otool is not None and tools.nm is not None
    try:
        otool_process = common.run_captured(
            (str(tools.otool), "-l", str(object_path)),
            timeout=timeout,
            environment=environment,
        )
        nm_process = common.run_captured(
            (str(tools.nm), "-g", "-U", str(object_path)),
            timeout=timeout,
            environment=environment,
        )
    except OSError as error:
        return {"object_bytes": object_bytes}, f"object_tool_os_error:{error}"
    for name, process in (("otool", otool_process), ("nm", nm_process)):
        (artifact_directory / f"{name}_stdout.txt").write_text(
            process.stdout, encoding="utf-8", errors="replace"
        )
        (artifact_directory / f"{name}_stderr.txt").write_text(
            process.stderr, encoding="utf-8", errors="replace"
        )
    if otool_process.timed_out or nm_process.timed_out:
        return {"object_bytes": object_bytes}, "object_tool_timeout"
    if otool_process.returncode != 0:
        return {"object_bytes": object_bytes}, (
            f"otool_exit_{otool_process.returncode}:"
            f"{common.normalized_excerpt(otool_process.stderr)}"
        )
    if nm_process.returncode != 0:
        return {"object_bytes": object_bytes}, (
            f"nm_exit_{nm_process.returncode}:"
            f"{common.normalized_excerpt(nm_process.stderr)}"
        )
    sections, section_error = parse_macho_sections(otool_process.stdout)
    values: dict[str, int | str] = {
        "object_bytes": object_bytes,
        "macho_text_bytes": sections["__text"],
        "macho_const_bytes": sections["__const"],
        "macho_cstring_bytes": sections["__cstring"],
    }
    if section_error:
        return values, section_error
    symbol_values, symbol_error = parse_nm_entry_point(
        nm_process.stdout, required_symbol
    )
    values.update(symbol_values)
    return values, symbol_error


def blank_row(
    run_id: str,
    sequence: int,
    revision: common.Revision,
    case: common.CompileCase,
    repeat: int,
    args: argparse.Namespace,
    requested_compiler: pathlib.Path,
    probe: CompilerProbe,
    sysroot: pathlib.Path,
    environment_policy_json: str,
    environment_policy_sha256: str,
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
            "compiler": "clang",
            "compiler_requested_path": str(requested_compiler),
            "compiler_path": str(probe.path) if probe.path else "",
            "compiler_family": "clang",
            "compiler_major": probe.major if probe.major is not None else "",
            "compiler_version": probe.version,
            "standard": args.standard,
            "sysroot": str(sysroot),
            "march": "native",
            "fuse_ld": args.fuse_ld or "",
            "environment_policy_json": environment_policy_json,
            "environment_policy_sha256": environment_policy_sha256,
            "case": case.case_id,
            "case_family": case.family,
            "pack_size": case.pack_size if case.pack_size is not None else "",
            "repeat": repeat,
        }
    )
    return row


def write_row(
    writer: csv.DictWriter,
    stream: object,
    row: dict[str, object],
) -> None:
    row["timestamp_utc"] = common.utc_timestamp()
    writer.writerow(row)
    # Every completed CSV row is a recovery boundary for a long serial matrix.
    stream.flush()  # type: ignore[attr-defined]


def execute_matrix(
    args: argparse.Namespace,
    revisions: Sequence[common.Revision],
    cases: Sequence[common.CompileCase],
    extensions: dict[tuple[str, str | None], pathlib.Path],
    extension_symbols: dict[tuple[str, str | None], str],
    sysroot: pathlib.Path,
    sysroot_error: str,
) -> int:
    requested_compiler = normalize_requested_compiler(args.compiler)
    tools = resolve_tools()
    run_id = uuid.uuid4().hex
    work_root = pathlib.Path(
        tempfile.mkdtemp(prefix="fast_io_compile_cost_darwin.", dir="/tmp")
    )
    environment, environment_policy_json, environment_policy_sha256 = (
        make_sanitized_environment(work_root)
    )
    (work_root / "environment_policy.json").write_text(
        environment_policy_json + "\n", encoding="utf-8"
    )
    frozen_sources = freeze_source_materials(
        cases, revisions, extensions, extension_symbols
    )
    probe = probe_compiler(requested_compiler, environment)
    (work_root / "compiler_probe.txt").write_text(
        probe.version + ("\n" if probe.version else ""), encoding="utf-8"
    )
    output_path = pathlib.Path(args.output).expanduser().resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    mode = "w" if args.overwrite else "x"
    counts = {"PASS": 0, "FAIL": 0, "SKIP": 0}
    sequence = 0
    with output_path.open(mode, newline="", encoding="utf-8") as csv_stream:
        writer = csv.DictWriter(csv_stream, fieldnames=CSV_FIELDS)
        writer.writeheader()
        csv_stream.flush()
        for case in cases:
            for revision, repeat in strict_revision_order(revisions):
                sequence += 1
                row = blank_row(
                    run_id,
                    sequence,
                    revision,
                    case,
                    repeat,
                    args,
                    requested_compiler,
                    probe,
                    sysroot,
                    environment_policy_json,
                    environment_policy_sha256,
                )
                if revision.error:
                    row.update(status="FAIL", reason=revision.error)
                elif sysroot_error:
                    row.update(status="FAIL", reason=sysroot_error)
                else:
                    resolution = frozen_sources[(case.case_id, revision.label)]
                    material = resolution.material
                    if resolution.status:
                        row.update(status=resolution.status, reason=resolution.reason)
                    elif material is not None and (
                        mutation_error := verify_frozen_source(material)
                    ):
                        row.update(status="FAIL", reason=mutation_error)
                    elif probe.error:
                        row.update(status="FAIL", reason=probe.error)
                    elif tools.error:
                        row.update(status="FAIL", reason=tools.error)
                    else:
                        assert material is not None
                        assert revision.include_root is not None
                        assert probe.path is not None
                        case_directory = work_root / (
                            f"{sequence:05d}_{case.case_id}_repeat{repeat}_{revision.label}"
                        )
                        case_directory.mkdir(parents=True, exist_ok=False)
                        source_path = case_directory / "case.cc"
                        object_path = case_directory / "case.o"
                        syntax_time_path = case_directory / "syntax_time.txt"
                        object_time_path = case_directory / "object_time.txt"
                        syntax_module_cache = case_directory / "syntax_module_cache"
                        object_module_cache = case_directory / "object_module_cache"
                        source_path.write_bytes(material.content)
                        syntax_command = build_syntax_command(
                            args,
                            probe.path,
                            revision.include_root,
                            source_path,
                            syntax_time_path,
                            material.origin,
                            sysroot,
                            syntax_module_cache,
                        )
                        object_command = build_object_command(
                            args,
                            probe.path,
                            revision.include_root,
                            source_path,
                            object_path,
                            object_time_path,
                            material.origin,
                            sysroot,
                            object_module_cache,
                        )
                        comparison_replacements = {
                            str(case_directory): "<case-artifact>",
                            str(revision.include_root): "<include-root>",
                        }
                        if material.origin is not None:
                            comparison_replacements[str(material.origin.parent)] = (
                                "<extension-directory>"
                            )
                        row.update(
                            source_sha256=common.sha256_bytes(material.content),
                            entry_point_symbol=material.entry_point_symbol,
                            artifact_directory=str(case_directory),
                            **command_fields(
                                "syntax", syntax_command, comparison_replacements
                            ),
                            **command_fields(
                                "object", object_command, comparison_replacements
                            ),
                        )
                        diagnostics: list[str] = []
                        try:
                            syntax_process = common.run_captured(
                                syntax_command,
                                timeout=args.timeout_seconds,
                                environment=environment,
                            )
                        except OSError as error:
                            row.update(
                                status="FAIL", reason=f"syntax_process_os_error:{error}"
                            )
                        else:
                            (case_directory / "syntax_stdout.txt").write_text(
                                syntax_process.stdout, encoding="utf-8", errors="replace"
                            )
                            (case_directory / "syntax_stderr.txt").write_text(
                                syntax_process.stderr, encoding="utf-8", errors="replace"
                            )
                            diagnostics.append(
                                "[syntax] " + syntax_process.stdout + "\n" + syntax_process.stderr
                            )
                            syntax_values, syntax_time_error = parse_darwin_time(
                                syntax_time_path
                            )
                            record_time_metrics(
                                row, "syntax", syntax_values, syntax_process.returncode
                            )
                            syntax_error = compiler_pass_error(
                                "syntax", syntax_process, syntax_time_error
                            )
                            if syntax_error:
                                row.update(status="FAIL", reason=syntax_error)
                            else:
                                try:
                                    object_process = common.run_captured(
                                        object_command,
                                        timeout=args.timeout_seconds,
                                        environment=environment,
                                    )
                                except OSError as error:
                                    row.update(
                                        status="FAIL",
                                        reason=f"object_process_os_error:{error}",
                                    )
                                else:
                                    (case_directory / "object_stdout.txt").write_text(
                                        object_process.stdout,
                                        encoding="utf-8",
                                        errors="replace",
                                    )
                                    (case_directory / "object_stderr.txt").write_text(
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
                                    object_values, object_time_error = parse_darwin_time(
                                        object_time_path
                                    )
                                    record_time_metrics(
                                        row,
                                        "object",
                                        object_values,
                                        object_process.returncode,
                                    )
                                    object_error = compiler_pass_error(
                                        "object", object_process, object_time_error
                                    )
                                    if object_error:
                                        row.update(status="FAIL", reason=object_error)
                                    else:
                                        measured, measure_error = measure_object(
                                            object_path,
                                            material.entry_point_symbol,
                                            tools,
                                            environment,
                                            args.timeout_seconds,
                                            case_directory,
                                        )
                                        row.update(measured)
                                        row.update(
                                            status="FAIL" if measure_error else "PASS",
                                            reason=measure_error,
                                        )
                        # The copied TU is immutable even if the original moves,
                        # but mutation invalidates the declared ABBA provenance.
                        post_mutation_error = verify_frozen_source(material)
                        if post_mutation_error:
                            row.update(status="FAIL", reason=post_mutation_error)
                        row["diagnostic_excerpt"] = common.normalized_excerpt(
                            "\n".join(diagnostics)
                        )
                counts[str(row["status"])] += 1
                write_row(writer, csv_stream, row)
    print(
        f"wrote {sequence} rows to {output_path}: "
        f"PASS={counts['PASS']} FAIL={counts['FAIL']} SKIP={counts['SKIP']}"
    )
    print(f"artifacts retained under {work_root}")
    return 1 if counts["FAIL"] else 0


def dry_run_matrix(
    args: argparse.Namespace,
    revisions: Sequence[common.Revision],
    cases: Sequence[common.CompileCase],
    extensions: dict[tuple[str, str | None], pathlib.Path],
    extension_symbols: dict[tuple[str, str | None], str],
    sysroot: pathlib.Path,
    sysroot_error: str,
) -> int:
    """Render the exact ABBA plan and argv without probing or invoking Clang."""

    requested = normalize_requested_compiler(args.compiler)
    compiler_path = common.resolve_program(str(requested))
    planned_root = pathlib.Path("/tmp/fast_io_compile_cost_darwin.DRY_RUN")
    _, environment_policy_json, environment_policy_sha256 = (
        make_sanitized_environment(planned_root, materialize=False)
    )
    frozen_sources = freeze_source_materials(
        cases, revisions, extensions, extension_symbols
    )
    sequence = 0
    has_failure = False
    print(f"artifact_root={planned_root} (not created)")
    print("execution_order=old,new,new,old")
    print(f"environment_policy_sha256={environment_policy_sha256}")
    print(f"environment_policy_json={environment_policy_json}")
    for case in cases:
        for revision, repeat in strict_revision_order(revisions):
            sequence += 1
            status = "PLAN"
            reason = ""
            material: SourceMaterial | None = None
            if revision.error:
                status, reason = "FAIL", revision.error
            elif sysroot_error:
                status, reason = "FAIL", sysroot_error
            else:
                resolution = frozen_sources[(case.case_id, revision.label)]
                material = resolution.material
                if resolution.status:
                    status, reason = resolution.status, resolution.reason
                elif material is not None and (
                    mutation_error := verify_frozen_source(material)
                ):
                    status, reason = "FAIL", mutation_error
                elif compiler_path is None:
                    status, reason = "FAIL", "explicit_compiler_unavailable"
            has_failure = has_failure or status == "FAIL"
            print(
                f"{sequence:04d} clang {case.case_id:22s} repeat={repeat} "
                f"revision={revision.label} status={status} {reason}"
            )
            if status != "PLAN":
                continue
            assert material is not None
            assert revision.include_root is not None
            assert compiler_path is not None
            case_directory = planned_root / (
                f"{sequence:05d}_{case.case_id}_repeat{repeat}_{revision.label}"
            )
            source_path = case_directory / "case.cc"
            syntax_command = build_syntax_command(
                args,
                compiler_path,
                revision.include_root,
                source_path,
                case_directory / "syntax_time.txt",
                material.origin,
                sysroot,
                case_directory / "syntax_module_cache",
            )
            object_command = build_object_command(
                args,
                compiler_path,
                revision.include_root,
                source_path,
                case_directory / "case.o",
                case_directory / "object_time.txt",
                material.origin,
                sysroot,
                case_directory / "object_module_cache",
            )
            comparison_replacements = {
                str(case_directory): "<case-artifact>",
                str(revision.include_root): "<include-root>",
            }
            if material.origin is not None:
                comparison_replacements[str(material.origin.parent)] = (
                    "<extension-directory>"
                )
            print(
                "  source_sha256=" + common.sha256_bytes(material.content)
            )
            for prefix, command in (("syntax", syntax_command), ("object", object_command)):
                fields = command_fields(prefix, command, comparison_replacements)
                print(f"  {prefix}_sha256={fields[f'{prefix}_command_sha256']}")
                print(
                    f"  {prefix}_comparable_sha256="
                    f"{fields[f'{prefix}_comparable_command_sha256']}"
                )
                print(f"  {prefix}_argv={fields[f'{prefix}_command_json']}")
    return 1 if has_failure else 0


def make_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Measure one serial Darwin Clang old/new compile-cost matrix with a fixed ABBA order."
        )
    )
    parser.add_argument(
        "--include-root",
        action="append",
        default=[],
        metavar="LABEL=PATH",
        help=(
            "exactly old=PATH and new=PATH; defaults to ../fast_io and the current repository"
        ),
    )
    parser.add_argument(
        "--compiler",
        default=str(DEFAULT_CLANG),
        metavar="ABSOLUTE_CLANG_PATH",
        help=f"single explicit Clang driver (default: {DEFAULT_CLANG})",
    )
    parser.add_argument(
        "--sysroot",
        default=str(DEFAULT_SYSROOT),
        metavar="SDK_PATH",
        help=f"required SDK passed as --sysroot=PATH (default: {DEFAULT_SYSROOT})",
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
        help="supply a complete TU for a shared extension slot",
    )
    parser.add_argument(
        "--extension-symbol",
        action="append",
        default=[],
        metavar="SLOT[@REVISION]=SYMBOL",
        help="required exact extern-C text symbol for entry-point retention",
    )
    parser.add_argument("--standard", choices=("c++20", "c++23"), default="c++20")
    parser.add_argument(
        "--fuse-ld",
        choices=("lld",),
        help=(
            "record and pass -fuse-ld=lld; no measured pass links an executable"
        ),
    )
    parser.add_argument(
        "--cxxflag",
        action="append",
        default=[],
        help="repeatable non-invariant compiler flag applied to both passes",
    )
    parser.add_argument(
        "--syntax-flag",
        action="append",
        default=[],
        help="repeatable non-invariant flag used only by -fsyntax-only",
    )
    parser.add_argument(
        "--object-flag",
        action="append",
        default=[],
        help="repeatable non-invariant flag used only by the O2 object pass",
    )
    parser.add_argument(
        "--timeout-seconds",
        type=float,
        default=DEFAULT_TIMEOUT_SECONDS,
        help=(
            "per compiler/object-tool process timeout; default 120 seconds for heavy templates"
        ),
    )
    parser.add_argument("--output", help="new CSV path; required for execution")
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="replace an existing CSV instead of refusing the run",
    )
    parser.add_argument("--list-cases", action="store_true")
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="print exact commands, hashes, and statuses without invoking Clang",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = make_argument_parser()
    args = parser.parse_args(argv)
    all_cases = common.builtin_cases()
    if args.list_cases:
        for case in all_cases:
            source_kind = "extension" if case.extension_slot else "built-in"
            pack = f" pack={case.pack_size}" if case.pack_size is not None else ""
            print(
                f"{case.case_id:22s} family={case.family:10s} "
                f"source={source_kind}{pack}"
            )
        return 0
    try:
        if args.timeout_seconds <= 0.0:
            raise ValueError("--timeout-seconds must be positive")
        validate_experiment_flags(args)
        normalize_requested_compiler(args.compiler)
        revisions = common.parse_revisions(args.include_root)
        strict_revision_order(revisions)
        revision_labels = {revision.label for revision in revisions}
        extensions = common.parse_extensions(args.extension_source, revision_labels)
        extension_symbols = common.parse_extension_symbols(
            args.extension_symbol, revision_labels
        )
        known_cases = {case.case_id for case in all_cases}
        unknown_cases = set(args.case).difference(known_cases)
        if unknown_cases:
            raise ValueError("unknown --case IDs: " + ",".join(sorted(unknown_cases)))
        selected = set(args.case)
        cases = (
            tuple(case for case in all_cases if case.case_id in selected)
            if selected
            else all_cases
        )
        sysroot, sysroot_error = normalize_sysroot(args.sysroot)
    except ValueError as error:
        parser.error(str(error))
    if args.dry_run:
        return dry_run_matrix(
            args,
            revisions,
            cases,
            extensions,
            extension_symbols,
            sysroot,
            sysroot_error,
        )
    if platform.system() != "Darwin":
        parser.error(
            "compiler execution is Darwin-only; use --list-cases or --dry-run elsewhere"
        )
    if not args.output:
        parser.error("--output is required for execution")
    output_path = pathlib.Path(args.output).expanduser().resolve(strict=False)
    if output_path.exists() and not args.overwrite:
        parser.error(f"output already exists: {output_path}; pass --overwrite explicitly")
    return execute_matrix(
        args,
        revisions,
        cases,
        extensions,
        extension_symbols,
        sysroot,
        sysroot_error,
    )


if __name__ == "__main__":
    raise SystemExit(main())
