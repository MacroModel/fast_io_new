#!/usr/bin/env python3
"""Build and run the finite-transmit staging A/B matrix on reserved Linux CPUs."""

from __future__ import annotations

import argparse
import csv
import os
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import sys
import tempfile


COMPILERS = ("g++-16", "clang++-22")
VARIANTS = ("N", "P", "old")
WIDTHS = (1, 2)
KINDS = (0, 1, 2, 3)
REQUESTS = (1, 3, 7, 4095, 4096, 4097)
ROUND_ORDERS = (
    ("N", "P", "old"),
    ("old", "P", "N"),
    ("P", "N", "old"),
    ("old", "N", "P"),
    ("P", "old", "N"),
    ("N", "old", "P"),
    ("N", "P", "old"),
)
TARGET_NS = 80_000_000
HARD_TIMEOUT_SECONDS = 0.78


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--new-root", type=Path, required=True)
    parser.add_argument("--old-root", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    return parser.parse_args()


def checked_run(command: list[str], **kwargs: object) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, check=True, text=True, **kwargs)


def copy_snapshots(new_root: Path, old_root: Path, work: Path) -> dict[str, Path]:
    snapshots = work / "snapshots"
    n_include = snapshots / "N" / "include"
    p_include = snapshots / "P" / "include"
    old_include = snapshots / "old" / "include"
    shutil.copytree(new_root / "include", n_include)
    shutil.copytree(n_include, p_include)
    shutil.copytree(old_root / "include", old_include)

    patch_file = (
        new_root
        / "benchmark"
        / "0025.old_new_cpo"
        / "finite_staging_fixed_allocation.patch"
    )
    checked_run(
        ["patch", "--batch", "--forward", "-p1", "-d", str(snapshots / "P")],
        stdin=patch_file.open("r", encoding="utf-8"),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    return {"N": n_include, "P": p_include, "old": old_include}


def compiler_id(compiler: str) -> str:
    return compiler.replace("+", "p").replace("-", "_")


def binary_name(compiler: str, variant: str, width: int, kind: int) -> str:
    return f"{compiler_id(compiler)}-{variant}-w{width}-k{kind}"


def text_size(binary: Path) -> int:
    result = checked_run(["size", "-A", str(binary)], stdout=subprocess.PIPE)
    for line in result.stdout.splitlines():
        fields = line.split()
        if len(fields) >= 2 and fields[0] == ".text":
            return int(fields[1])
    raise RuntimeError(f".text not found in {binary}")


def build_matrix(
    new_root: Path, work: Path, includes: dict[str, Path]
) -> tuple[dict[tuple[str, str, int, int], Path], list[dict[str, object]]]:
    source = (
        new_root
        / "benchmark"
        / "0025.old_new_cpo"
        / "finite_staging_ab.cc"
    )
    build_dir = work / "bin"
    log_dir = work / "compile-logs"
    build_dir.mkdir(parents=True)
    log_dir.mkdir(parents=True)
    binaries: dict[tuple[str, str, int, int], Path] = {}
    rows: list[dict[str, object]] = []

    for compiler in COMPILERS:
        version = checked_run(
            [compiler, "--version"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT
        ).stdout.splitlines()[0]
        for variant in VARIANTS:
            for width in WIDTHS:
                for kind in KINDS:
                    name = binary_name(compiler, variant, width, kind)
                    binary = build_dir / name
                    metric = log_dir / f"{name}.time"
                    log = log_dir / f"{name}.log"
                    min_policy = "1" if variant == "N" else "0"
                    command = [
                        "taskset",
                        "-c",
                        "16",
                        "/usr/bin/time",
                        "-f",
                        "%e\t%M",
                        "-o",
                        str(metric),
                        compiler,
                        "-O3",
                        "-march=native",
                        "-std=c++20",
                        "-DNDEBUG",
                        f"-DFAST_IO_FINITE_STAGING_CHAR_WIDTH={width}",
                        f"-DFAST_IO_FINITE_STAGING_KIND={kind}",
                        f"-DFAST_IO_FINITE_STAGING_MIN_POLICY={min_policy}",
                        f"-I{includes[variant]}",
                        str(source),
                        "-o",
                        str(binary),
                    ]
                    with log.open("w", encoding="utf-8") as log_stream:
                        result = subprocess.run(
                            command,
                            text=True,
                            stdout=log_stream,
                            stderr=subprocess.STDOUT,
                        )
                    wall = ""
                    rss = ""
                    if metric.exists():
                        fields = metric.read_text(encoding="utf-8").strip().split()
                        if len(fields) >= 2:
                            wall, rss = fields[0], fields[1]
                    row: dict[str, object] = {
                        "compiler": compiler,
                        "compiler_version": version,
                        "variant": variant,
                        "char_width": width,
                        "kind": kind,
                        "status": result.returncode,
                        "compile_wall_s": wall,
                        "compile_max_rss_kb": rss,
                        "text_bytes": "",
                        "elf_bytes": "",
                        "log": log,
                    }
                    if result.returncode == 0:
                        row["text_bytes"] = text_size(binary)
                        row["elf_bytes"] = binary.stat().st_size
                        binaries[(compiler, variant, width, kind)] = binary
                    rows.append(row)
    return binaries, rows


def cpu15_ticks() -> tuple[int, int]:
    """Return conventional busy and total scheduler ticks for the SMT sibling."""
    with Path("/proc/stat").open("r", encoding="ascii") as stream:
        for line in stream:
            if line.startswith("cpu15 "):
                values = [int(field) for field in line.split()[1:]]
                while len(values) < 8:
                    values.append(0)
                user, nice, system, idle, iowait, irq, softirq, steal = values[:8]
                busy = user + nice + system + irq + softirq + steal
                total = busy + idle + iowait
                return busy, total
    raise RuntimeError("cpu15 is absent from /proc/stat")


KEY_VALUE = re.compile(r"([a-z_]+)=([^ ]+)")


def run_one(binary: Path, request: int, iterations: int) -> dict[str, object]:
    busy_before, total_before = cpu15_ticks()
    try:
        result = subprocess.run(
            ["taskset", "-c", "14", str(binary), str(request), str(iterations)],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=HARD_TIMEOUT_SECONDS,
        )
        timed_out = False
    except subprocess.TimeoutExpired as error:
        result = None
        timed_out = True
        stdout = error.stdout or ""
        stderr = error.stderr or ""
    busy_after, total_after = cpu15_ticks()
    if result is not None:
        stdout = result.stdout.strip()
        stderr = result.stderr.strip()
        return_code = result.returncode
    else:
        return_code = 124
    parsed = {match.group(1): match.group(2) for match in KEY_VALUE.finditer(stdout)}
    return {
        "return_code": return_code,
        "timed_out": int(timed_out),
        "stdout": stdout,
        "stderr": stderr,
        "cpu15_busy_before": busy_before,
        "cpu15_busy_after": busy_after,
        "cpu15_busy_ticks": busy_after - busy_before,
        "cpu15_total_before": total_before,
        "cpu15_total_after": total_after,
        "cpu15_total_ticks": total_after - total_before,
        **parsed,
    }


def calibrate(binary: Path, request: int) -> tuple[int, list[dict[str, object]]]:
    iterations = 1_000
    observations: list[dict[str, object]] = []
    for attempt in range(4):
        observation = run_one(binary, request, iterations)
        observation["attempt"] = attempt
        observations.append(observation)
        if observation["return_code"] != 0 or "elapsed_ns" not in observation:
            raise RuntimeError(
                f"calibration failed for {binary.name} request={request}: {observation}"
            )
        elapsed = int(str(observation["elapsed_ns"]))
        if elapsed >= 2_000_000:
            break
        scale = max(2, min(100, (5_000_000 + max(elapsed, 1) - 1) // max(elapsed, 1)))
        iterations = min(50_000_000, iterations * scale)
    elapsed = int(str(observations[-1]["elapsed_ns"]))
    measured_iterations = int(str(observations[-1]["iterations"]))
    target_iterations = max(
        1,
        min(
            50_000_000,
            int(round(measured_iterations * TARGET_NS / max(elapsed, 1))),
        ),
    )
    return target_iterations, observations


def write_rows(path: Path, rows: list[dict[str, object]]) -> None:
    fieldnames: list[str] = []
    for row in rows:
        for key in row:
            if key not in fieldnames:
                fieldnames.append(key)
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def benchmark_matrix(
    binaries: dict[tuple[str, str, int, int], Path]
) -> tuple[list[dict[str, object]], list[dict[str, object]]]:
    calibrations: list[dict[str, object]] = []
    samples: list[dict[str, object]] = []
    iteration_counts: dict[tuple[str, str, int, int, int], int] = {}
    for compiler in COMPILERS:
        for width in WIDTHS:
            for kind in KINDS:
                available_variants = [
                    variant
                    for variant in VARIANTS
                    if (compiler, variant, width, kind) in binaries
                ]
                for request in REQUESTS:
                    for variant in available_variants:
                        key = (compiler, variant, width, kind, request)
                        count, observations = calibrate(
                            binaries[(compiler, variant, width, kind)], request
                        )
                        iteration_counts[key] = count
                        for observation in observations:
                            calibrations.append(
                                {
                                    "compiler": compiler,
                                    "variant": variant,
                                    "char_width": width,
                                    "kind": kind,
                                    "request": request,
                                    **observation,
                                }
                            )
                    for round_index, order in enumerate(ROUND_ORDERS):
                        for sequence, variant in enumerate(order):
                            if variant not in available_variants:
                                continue
                            key = (compiler, variant, width, kind, request)
                            observation = run_one(
                                binaries[(compiler, variant, width, kind)],
                                request,
                                iteration_counts[key],
                            )
                            samples.append(
                                {
                                    "compiler": compiler,
                                    "variant": variant,
                                    "char_width": width,
                                    "kind": kind,
                                    "request": request,
                                    "round": round_index,
                                    "sequence": sequence,
                                    "target_ns": TARGET_NS,
                                    **observation,
                                }
                            )
                            if observation["return_code"] != 0:
                                raise RuntimeError(
                                    "timed sample failed: "
                                    f"{compiler}/{variant}/w{width}/k{kind}/"
                                    f"request={request}/round={round_index}: {observation}"
                                )
    return calibrations, samples


def summarize(samples: list[dict[str, object]]) -> list[dict[str, object]]:
    groups: dict[tuple[str, str, int, int, int], list[dict[str, object]]] = {}
    for row in samples:
        key = (
            str(row["compiler"]),
            str(row["variant"]),
            int(row["char_width"]),
            int(row["kind"]),
            int(row["request"]),
        )
        groups.setdefault(key, []).append(row)
    summary: list[dict[str, object]] = []
    for key, rows in groups.items():
        values = [float(str(row["ns_per_op"])) for row in rows]
        idle_values = [
            float(str(row["ns_per_op"]))
            for row in rows
            if int(row["cpu15_busy_ticks"]) == 0
        ]
        selected = idle_values if idle_values else values
        compiler, variant, width, kind, request = key
        summary.append(
            {
                "compiler": compiler,
                "variant": variant,
                "char_width": width,
                "kind": kind,
                "request": request,
                "samples": len(values),
                "cpu15_idle_samples": len(idle_values),
                "median_ns_per_op": f"{statistics.median(selected):.6f}",
                "min_ns_per_op": f"{min(selected):.6f}",
                "max_ns_per_op": f"{max(selected):.6f}",
                "cpu15_busy_ticks_total": sum(
                    int(row["cpu15_busy_ticks"]) for row in rows
                ),
            }
        )
    return summary


def main() -> int:
    arguments = parse_arguments()
    new_root = arguments.new_root.resolve()
    old_root = arguments.old_root.resolve()
    if arguments.output is None:
        work = Path(tempfile.mkdtemp(prefix="fast_io_finite_staging.", dir="/tmp"))
    else:
        work = arguments.output.resolve()
        work.mkdir(parents=True, exist_ok=False)
    if os.sched_getaffinity(0) != {0}:
        print(
            "runner must itself be pinned to CPU0; compile and timed children "
            "are separately pinned to CPU16 and CPU14",
            file=sys.stderr,
        )
        return 64

    includes = copy_snapshots(new_root, old_root, work)
    binaries, compile_rows = build_matrix(new_root, work, includes)
    write_rows(work / "compile.csv", compile_rows)
    calibrations, samples = benchmark_matrix(binaries)
    write_rows(work / "calibration.csv", calibrations)
    write_rows(work / "samples.csv", samples)
    write_rows(work / "summary.csv", summarize(samples))
    print(work)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

