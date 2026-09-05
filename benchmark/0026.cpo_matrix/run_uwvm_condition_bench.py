#!/usr/bin/env python3
"""Fresh, serial three-version builds and runs of uwvm_condition_bench.cc.

Example (Linux; inherit the toolchain's LD_LIBRARY_PATH):
  python3 run_uwvm_condition_bench.py SNAPSHOT BEFORE_INCLUDE AFTER_INCLUDE \
      OFFICIAL_INCLUDE ARTIFACT_DIRECTORY --cxx /absolute/path/to/clang++ --cpu 14

Every invocation creates a new artifact subdirectory and recompiles all nine
timed executables and nine full-byte checks from one frozen source copy.
No previously built binary is reused.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import shlex
import shutil
import signal
import statistics
import subprocess
import sys
import tempfile
import time


VERSIONS = ("before", "after", "official")
PROFILES = {"00": (0, 0), "01": (0, 1), "11": (1, 1)}
MODES = ("off", "on", "correlated", "independent")
RESULT = re.compile(
    r"(?P<ns>[0-9]+(?:\.[0-9]+)?) ns/record "
    r"checksum=(?P<checksum>[0-9]+) calls=(?P<calls>[0-9]+) "
    r"sample=(?P<sample>[0-9]+) mode=(?P<mode>[a-z]+) "
    r"predicate=(?P<predicate>[a-z]+) timestamp=(?P<timestamp>[01])"
)


def executable(value: str) -> str:
    found = shutil.which(value)
    if found is None:
        raise ValueError(f"executable not found: {value}")
    # Preserve the executable spelling: resolving clang++'s symlink to clang
    # changes its default language/linker mode despite the same target binary.
    return os.path.abspath(found)


def invoke(command: list[str], prefix: Path, timeout: float, vm_bytes: int | None = None) -> dict:
    stdout_path = prefix.with_suffix(".stdout")
    stderr_path = prefix.with_suffix(".stderr")

    def limit_memory() -> None:
        resource.setrlimit(resource.RLIMIT_AS, (vm_bytes, vm_bytes))

    start = time.monotonic()
    timed_out = False
    with stdout_path.open("w") as stdout, stderr_path.open("w") as stderr:
        process = subprocess.Popen(
            command,
            stdout=stdout,
            stderr=stderr,
            start_new_session=True,
            preexec_fn=limit_memory if vm_bytes is not None else None,
            env={**os.environ, "LC_ALL": "C"},
        )
        try:
            returncode = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            os.killpg(process.pid, signal.SIGTERM)
            try:
                returncode = process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                returncode = process.wait()
    return {
        "command": command,
        "command_shell": shlex.join(command),
        "stdout": str(stdout_path),
        "stderr": str(stderr_path),
        "returncode": returncode,
        "timed_out": timed_out,
        "elapsed_seconds": time.monotonic() - start,
    }


def require_success(result: dict) -> None:
    if result["returncode"] != 0 or result["timed_out"]:
        reason = "timed out" if result["timed_out"] else f"exit {result['returncode']}"
        raise RuntimeError(f"{reason}: {result['command_shell']} (log: {result['stderr']})")


def require_compiled(result: dict, binary: Path) -> None:
    require_success(result)
    diagnostics = Path(result["stderr"]).read_text()
    if re.search(r"(?:\berror:|LLVM ERROR:|out of memory)", diagnostics, re.IGNORECASE):
        raise RuntimeError(f"compiler reported an error despite exit zero: {result['stderr']}")
    if not binary.is_file() or binary.stat().st_size == 0:
        raise RuntimeError(f"compiler did not produce a nonempty executable: {binary}")


def time_resources(path: Path) -> dict:
    text = path.read_text()
    result = {"raw": str(path)}
    labels = {
        "user_seconds": r"User time \(seconds\): ([0-9.]+)",
        "system_seconds": r"System time \(seconds\): ([0-9.]+)",
        "peak_rss_kib": r"Maximum resident set size \(kbytes\): ([0-9]+)",
    }
    for name, pattern in labels.items():
        match = re.search(pattern, text)
        if match:
            result[name] = int(match[1]) if name == "peak_rss_kib" else float(match[1])
    match = re.search(r"Elapsed \(wall clock\) time \(h:mm:ss or m:ss\): ([0-9:.]+)", text)
    if match:
        seconds = 0.0
        for component in match[1].split(":"):
            seconds = seconds * 60.0 + float(component)
        result["wall_seconds"] = seconds
    return result


def summarize(document: dict) -> str:
    lines = [
        "# Three-version uwvm condition benchmark",
        "",
        f"Source SHA-256: `{document['source_sha256']}`. "
        f"CPU {document['cpu']}; {document['rounds']} rounds; "
        f"{document['iterations']:,} records per run.",
        "",
        "Profiles: 00 = independent predicates, no timestamp; "
        "01 = independent predicates with timestamp; 11 = shared predicate with timestamp.",
        "",
        "All three versions must match length checksum and sampled bytes. "
        "Before/after must also match primitive calls; official calls are recorded without requiring the same strategy.",
        "",
        "| Profile | Version | Compile s | Peak RSS MiB | Text bytes |",
        "| --- | --- | ---: | ---: | ---: |",
    ]
    for item in document["compiles"]:
        measured = item["resources"]
        lines.append(
            f"| {item['profile']} | {item['version']} | "
            f"{measured.get('wall_seconds', item['elapsed_seconds']):.2f} | "
            f"{measured.get('peak_rss_kib', 0) / 1024:.2f} | {item['size']['text']} |"
        )
    lines += [
        "",
        "Median ns/record; negative percentages mean the after version is faster.",
        "",
        "| Profile | Mode | Memory | Before | After | Official | After / before | After / official |",
        "| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |",
    ]
    medians = []
    for profile in PROFILES:
        for mode in MODES:
            for memory in ("null", "valid"):
                values = {}
                counts = {}
                for version in VERSIONS:
                    matching = [
                        run for run in document["runs"]
                        if (run["profile"], run["mode"], run["memory"], run["version"])
                        == (profile, mode, memory, version)
                    ]
                    samples = [run["ns"] for run in matching]
                    values[version] = statistics.median(samples)
                    counts[version] = sorted({run["calls"] for run in matching})
                before_change = (values["after"] / values["before"] - 1.0) * 100.0
                official_change = (values["after"] / values["official"] - 1.0) * 100.0
                medians.append({
                    "profile": profile, "mode": mode, "memory": memory,
                    "median_ns": values, "primitive_calls": counts,
                    "after_vs_before_percent": before_change,
                    "after_vs_official_percent": official_change,
                })
                lines.append(
                    f"| {profile} | {mode} | {memory} | {values['before']:.3f} | "
                    f"{values['after']:.3f} | {values['official']:.3f} | "
                    f"{before_change:+.1f}% | {official_change:+.1f}% |"
                )
    document["medians"] = medians
    lines += ["", f"Validated {len(document['comparisons'])} three-version run groups.", ""]
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("snapshot", type=Path)
    parser.add_argument("before_include", type=Path)
    parser.add_argument("after_include", type=Path)
    parser.add_argument("official_include", type=Path)
    parser.add_argument("artifact_directory", type=Path)
    parser.add_argument("--source", type=Path, default=Path(__file__).with_name("uwvm_condition_bench.cc"))
    parser.add_argument("--equivalence-source", type=Path, default=Path(__file__).with_name("uwvm_condition_equivalence.cc"))
    parser.add_argument("--cxx", default=os.environ.get("CXX", "clang++"))
    parser.add_argument("--cpu", type=int, default=int(os.environ.get("BENCH_CPU", os.environ.get("CPU", "14"))))
    parser.add_argument("--size-tool", default=os.environ.get("SIZE", "size"))
    parser.add_argument("--rounds", type=int, default=3)
    parser.add_argument("--iterations", type=int, default=3_000_000)
    parser.add_argument("--compile-timeout", type=float, default=600)
    parser.add_argument("--compile-vm-gib", type=float, default=32)
    parser.add_argument("--run-timeout", type=float, default=60)
    args = parser.parse_args()
    if min(args.rounds, args.iterations, args.compile_timeout, args.compile_vm_gib, args.run_timeout) <= 0:
        parser.error("rounds, iterations, timeouts and VM limit must be positive")
    if hasattr(os, "sched_getaffinity") and args.cpu not in os.sched_getaffinity(0):
        parser.error(f"CPU {args.cpu} is outside the current allowed affinity set")
    try:
        cxx = executable(args.cxx)
        taskset = executable("taskset")
        size_tool = executable(args.size_tool)
    except ValueError as error:
        parser.error(str(error))
    if not Path("/usr/bin/time").is_file():
        parser.error("GNU /usr/bin/time is required")

    includes = {version: getattr(args, f"{version}_include").resolve() for version in VERSIONS}
    snapshot = args.snapshot.resolve()
    common_includes = [snapshot / "src", snapshot / "third-parties/bizwen/include", snapshot / "third-parties/boost_unordered/include"]
    for path in [*includes.values(), *common_includes]:
        if not path.is_dir():
            parser.error(f"include directory does not exist: {path}")
    source = args.source.resolve()
    if not source.is_file():
        parser.error(f"source does not exist: {source}")
    equivalence_source = args.equivalence_source.resolve()
    if not equivalence_source.is_file():
        parser.error(f"equivalence source does not exist: {equivalence_source}")
    artifact_parent = args.artifact_directory.resolve()
    artifact_parent.mkdir(parents=True, exist_ok=True)
    artifact = Path(tempfile.mkdtemp(prefix="condition-bench-", dir=artifact_parent))
    frozen_source = artifact / "uwvm_condition_bench.cc"
    shutil.copyfile(source, frozen_source)
    frozen_equivalence = artifact / "uwvm_condition_equivalence.cc"
    shutil.copyfile(equivalence_source, frozen_equivalence)
    for name in ("bin", "compile", "runs"):
        (artifact / name).mkdir()
    document = {
        "status": "running", "artifact_directory": str(artifact),
        "source": str(source), "frozen_source": str(frozen_source),
        "source_sha256": hashlib.sha256(frozen_source.read_bytes()).hexdigest(),
        "equivalence_source": str(equivalence_source), "frozen_equivalence": str(frozen_equivalence),
        "equivalence_source_sha256": hashlib.sha256(frozen_equivalence.read_bytes()).hexdigest(),
        "snapshot": str(snapshot), "includes": {key: str(value) for key, value in includes.items()},
        "cpu": args.cpu, "cxx": cxx,
        "rounds": args.rounds, "iterations": args.iterations,
        "compile_timeout_seconds": args.compile_timeout, "compile_vm_gib": args.compile_vm_gib,
        "run_timeout_seconds": args.run_timeout,
        "environment": {**{key: os.environ[key] for key in ("LD_LIBRARY_PATH", "LANG") if key in os.environ}, "LC_ALL": "C"},
        "compiles": [], "equivalence_compiles": [], "equivalence_checks": [], "runs": [], "comparisons": [],
    }
    results_path = artifact / "results.json"

    def save() -> None:
        results_path.write_text(json.dumps(document, indent=2) + "\n")

    print(f"Artifacts: {artifact}", flush=True)
    try:
        version_result = invoke([cxx, "--version"], artifact / "compiler-version", 10)
        require_success(version_result)
        document["compiler_version"] = Path(version_result["stdout"]).read_text()
        flags = ["-std=c++26", "-O3", "-march=native", "-DUWVM=2", "-DUWVM_DISABLE_JIT", "-DUWVM_USE_DEFAULT_INT"]
        for profile, (shared, timestamp) in PROFILES.items():
            for version in VERSIONS:
                name = f"{version}-{profile}"
                binary = artifact / "bin" / name
                resources_path = artifact / "compile" / f"{name}.time"
                command = [
                    "/usr/bin/time", "-v", "-o", str(resources_path), taskset, "-c", str(args.cpu), cxx,
                    *flags, f"-DUWVM_BENCH_SHARED_PREDICATE={shared}", f"-DUWVM_BENCH_TIMESTAMP={timestamp}",
                    *(f"-I{path}" for path in [includes[version], *common_includes]),
                    str(frozen_source), "-o", str(binary),
                ]
                print(f"Compiling {name}", flush=True)
                compiled = invoke(command, artifact / "compile" / name, args.compile_timeout, int(args.compile_vm_gib * 1024**3))
                compiled.update({"profile": profile, "version": version, "binary": str(binary)})
                document["compiles"].append(compiled)
                save()
                require_compiled(compiled, binary)
                compiled["resources"] = time_resources(resources_path)
                sized = invoke([size_tool, str(binary)], artifact / "compile" / f"{name}-size", 10)
                require_success(sized)
                size_lines = Path(sized["stdout"]).read_text().strip().splitlines()
                columns = size_lines[-1].split()
                compiled["size"] = dict(zip(("text", "data", "bss"), map(int, columns[:3])))
                compiled["size_command"] = sized
                save()
        for profile, (shared, timestamp) in PROFILES.items():
            byte_outputs = {}
            checks = {}
            for version in VERSIONS:
                name = f"equivalence-{version}-{profile}"
                binary = artifact / "bin" / name
                resources_path = artifact / "compile" / f"{name}.time"
                # Use the identical optimizer/options/include roots as the timed
                # executable, with the full-byte workload wrapper as the TU.
                matching = next(item for item in document["compiles"] if (item["profile"], item["version"]) == (profile, version))
                command = matching["command"][:-3] + [str(frozen_equivalence), "-o", str(binary)]
                command[3] = str(resources_path)
                print(f"Compiling {name}", flush=True)
                compiled = invoke(command, artifact / "compile" / name, args.compile_timeout, int(args.compile_vm_gib * 1024**3))
                compiled.update({"profile": profile, "version": version, "binary": str(binary)})
                document["equivalence_compiles"].append(compiled)
                save()
                require_compiled(compiled, binary)
                compiled["resources"] = time_resources(resources_path)
                checked = invoke([taskset, "-c", str(args.cpu), str(binary)], artifact / "runs" / name, args.run_timeout)
                require_success(checked)
                byte_outputs[version] = Path(checked["stdout"]).read_bytes()
                metadata = re.fullmatch(r"records=([0-9]+) primitive_calls=([0-9]+) predicate=([a-z]+) timestamp=([01])", Path(checked["stderr"]).read_text().strip())
                if metadata is None or (int(metadata[1]), metadata[3], int(metadata[4])) != (512, "shared" if shared else "independent", timestamp):
                    raise RuntimeError(f"invalid full-byte contract result: {checked['stderr']}")
                if len(byte_outputs[version].splitlines()) != 512:
                    raise RuntimeError(f"incomplete full-byte contract output: {checked['stdout']}")
                checked.update({"sha256": hashlib.sha256(byte_outputs[version]).hexdigest(), "records": 512, "primitive_calls": int(metadata[2])})
                checks[version] = checked
            if byte_outputs["before"] != byte_outputs["after"] or byte_outputs["before"] != byte_outputs["official"]:
                raise RuntimeError(f"full-byte cross-version mismatch in profile {profile}; see equivalence stdout files")
            if checks["before"]["primitive_calls"] != checks["after"]["primitive_calls"]:
                raise RuntimeError(f"full-byte before/after CPO count mismatch in profile {profile}")
            document["equivalence_checks"].append({"profile": profile, "versions": checks, "equal": True})
            save()
            print(f"Full-byte equivalence passed: {profile}, 512 records per version", flush=True)
        for round_number in range(args.rounds):
            # Rotate version order to avoid always giving one version the first run.
            order = VERSIONS[round_number % 3:] + VERSIONS[:round_number % 3]
            for profile, (shared, timestamp) in PROFILES.items():
                for mode in MODES:
                    for memory in ("null", "valid"):
                        group = {}
                        for version in order:
                            name = f"r{round_number + 1}-{profile}-{mode}-{memory}-{version}"
                            binary = artifact / "bin" / f"{version}-{profile}"
                            run = invoke([taskset, "-c", str(args.cpu), str(binary), str(args.iterations), mode, memory], artifact / "runs" / name, args.run_timeout)
                            run.update({"round": round_number + 1, "profile": profile, "mode": mode, "memory": memory, "version": version})
                            document["runs"].append(run)
                            require_success(run)
                            output = Path(run["stdout"]).read_text().strip()
                            match = RESULT.fullmatch(output)
                            if match is None:
                                raise RuntimeError(f"unrecognized benchmark output: {run['stdout']}")
                            actual = match.groupdict()
                            expected_predicate = "shared" if shared else "independent"
                            if (actual["mode"], actual["predicate"], int(actual["timestamp"])) != (mode, expected_predicate, timestamp):
                                raise RuntimeError(f"wrong executable configuration: {run['stdout']}")
                            run.update({key: int(actual[key]) for key in ("checksum", "calls", "sample")})
                            run["ns"] = float(actual["ns"])
                            group[version] = run
                        for field in ("checksum", "sample"):
                            if len({run[field] for run in group.values()}) != 1:
                                raise RuntimeError(f"cross-version {field} mismatch: round {round_number + 1}, {profile}/{mode}/{memory}")
                        if group["before"]["calls"] != group["after"]["calls"]:
                            raise RuntimeError(f"before/after CPO call mismatch: round {round_number + 1}, {profile}/{mode}/{memory}")
                        document["comparisons"].append({
                            "round": round_number + 1, "profile": profile, "mode": mode, "memory": memory,
                            "checksum": group["before"]["checksum"], "sample": group["before"]["sample"],
                            "primitive_calls": {version: group[version]["calls"] for version in VERSIONS},
                        })
                        save()
                print(f"Round {round_number + 1}/{args.rounds}, profile {profile} complete", flush=True)
        summary = summarize(document)
        (artifact / "summary.md").write_text(summary)
        document["status"] = "complete"
        save()
        print(summary, end="")
        print(f"JSON: {results_path}")
        return 0
    except (RuntimeError, ValueError, OSError) as error:
        document["status"] = "failed"
        document["error"] = str(error)
        save()
        print(f"ERROR: {error}\nJSON: {results_path}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
