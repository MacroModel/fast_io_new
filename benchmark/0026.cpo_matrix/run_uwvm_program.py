#!/usr/bin/env python3
"""Finish an isolated uwvm2 main-object run and check executable behavior on Linux.

Input is a successful object run from run_uwvm_compile.sh. All new objects,
executables, Wasm fixtures, commands, resource reports and diagnostics go into a
fresh artifact directory; the supplied source/include snapshots are read only.
The runtime TU retains -O3 and receives the four FP flags from uwvm2/xmake.lua.
No xmake, wat2wasm, network access or source-tree fixture regeneration is needed.

Example (preserve the compiler's LD_LIBRARY_PATH in the calling environment):
  python3 run_uwvm_program.py /disk/compile.XXXXXX --cpu 14
  python3 run_uwvm_program.py /disk/compile.XXXXXX --cpu 14 --integration-only

Integration-only defaults to RUN_DIR/uwvm; pass --binary to check another
already-linked executable. It performs no compilation or linking.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tempfile


RUNTIME_FP_FLAGS = (
    "-fno-math-errno", "-fno-trapping-math", "-fno-rounding-math", "-ffp-contract=off",
)
COMPILER_FAILURE = re.compile(r"(?:^|\s)(?:LLVM ERROR:|fatal error:|error:)|out of memory")
ANSI_SGR = re.compile(rb"\x1b\[[0-9;]*m")
MISMATCHES = (
    ("consumer_bad_func", ("i64",), "i64", "(i64) -> i64"),
    ("consumer_bad_func_result", ("i32",), "i64", "(i32) -> i64"),
    ("consumer_bad_func_param_count", ("i32", "i32"), "i32", "(i32, i32) -> i32"),
)


def digest(path: Path) -> str:
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def read_main_run(run: Path) -> tuple[list[str], Path, Path]:
    command = shlex.split((run / "command.txt").read_text())
    if not command or command.count("-c") != 1 or command.count("-o") != 1:
        raise ValueError("command.txt must describe one object compile from run_uwvm_compile.sh")
    if "-fsyntax-only" in command or command[-1].endswith("main.module.cpp"):
        raise ValueError("A successful non-module main.default.cpp object run is required")
    source = Path(command[-1])
    if not source.is_absolute() or source.name != "main.default.cpp" or not source.is_file():
        raise ValueError("The recorded absolute main.default.cpp source must still exist")
    if not Path(command[0]).is_absolute() or not Path(command[0]).is_file():
        raise ValueError("The recorded compiler must be an existing absolute executable path")
    optimization = [arg for arg in command if re.fullmatch(r"-O(?:[0-3sgz]|fast)?", arg)]
    if not optimization or optimization[-1] != "-O3":
        raise ValueError("The recorded effective optimization level must be -O3")
    standard = [arg for arg in command if arg.startswith("-std=")]
    if not standard or standard[-1] != "-std=c++26":
        raise ValueError("The recorded effective language mode must be -std=c++26")
    target = Path(command[command.index("-o") + 1])
    if target.resolve() != run / "main.o" or not target.is_file() or not target.stat().st_size:
        raise ValueError("The recorded output must be a nonempty RUN_DIR/main.o")
    if COMPILER_FAILURE.search((run / "compiler.log").read_text(errors="replace")):
        raise ValueError("The main compiler log contains a failure diagnostic")
    resources = (run / "resources.txt").read_text()
    if not re.search(r"^\s*Exit status: 0\s*$", resources, re.MULTILINE):
        raise ValueError("The main compile resource report does not record exit status 0")
    return command, source, target


def link_flags(command: list[str]) -> list[str]:
    """Keep driver/code-generation flags, dropping this runner's compile-only inputs."""
    result = []
    paired = {"-o", "-I", "-D", "-U", "-include", "-imacros", "-isystem", "-iquote", "-idirafter"}
    index = 1
    while index < len(command) - 1:
        arg = command[index]
        if arg in paired:
            index += 2
            continue
        if arg == "-c" or arg.startswith(("-I", "-D", "-U", "-std=")):
            index += 1
            continue
        result.append(arg)
        index += 1
    return result


def execute(args: argparse.Namespace, output: Path, name: str, command: list[str], *, build: bool) -> int:
    """Bound every subprocess and retain both its direct command and wrapper command."""
    timeout = args.compile_timeout if build else args.run_timeout
    wrapped = [
        "/usr/bin/time", "-v", "-o", str(output / f"{name}.resources.txt"),
        "timeout", "--kill-after=5s", str(timeout),
        "taskset", "-c", str(args.cpu), "prlimit", f"--as={args.max_virtual_bytes}", "--core=0", "--",
        *command,
    ]
    (output / f"{name}.command.txt").write_text(shlex.join(command) + "\n")
    (output / f"{name}.wrapped-command.txt").write_text(shlex.join(wrapped) + "\n")
    with (output / f"{name}.stdout").open("wb") as stdout, (output / f"{name}.stderr").open("wb") as stderr:
        status = subprocess.run(wrapped, cwd=output, stdout=stdout, stderr=stderr).returncode
    (output / f"{name}.status.txt").write_text(str(status) + "\n")
    return status


def build_program(args: argparse.Namespace, output: Path, command: list[str], source: Path, main: Path) -> Path:
    objects = [main]
    units = (
        ("host_api", source.with_name("host_api.default.cpp"), ()),
        ("runtime", source.parents[1] / "runtime/lib/uwvm_runtime.default.cpp", RUNTIME_FP_FLAGS),
    )
    for name, unit, extra in units:
        if not unit.is_file():
            raise ValueError(f"Missing translation unit: {unit}")
        target = output / f"{name}.o"
        derived = command.copy()
        derived[derived.index("-o") + 1] = str(target)
        # Append the runtime flags after the original options so a conflicting
        # earlier FP option cannot silently override the runtime's exact policy.
        derived[-1:] = [*extra, str(unit)]
        print(f"Compiling {name}", flush=True)
        status = execute(args, output, name, derived, build=True)
        diagnostics = (output / f"{name}.stderr").read_text(errors="replace")
        diagnostics += (output / f"{name}.stdout").read_text(errors="replace")
        if status or COMPILER_FAILURE.search(diagnostics) or not target.is_file() or not target.stat().st_size:
            raise RuntimeError(f"{name} compile failed (exit {status}); inspect {output / (name + '.stderr')}")
        objects.append(target)
    binary = output / "uwvm"
    link = [command[0], *link_flags(command), *map(str, objects), "-pthread", "-latomic", "-ldl", "-o", str(binary)]
    if execute(args, output, "link", link, build=True) or not binary.is_file():
        raise RuntimeError(f"Link failed; inspect {output / 'link.stderr'}")
    return binary


def uleb(value: int) -> bytes:
    result = bytearray()
    while True:
        byte = value & 127
        value >>= 7
        result.append(byte | (128 if value else 0))
        if not value:
            return bytes(result)


def vector(items: list[bytes]) -> bytes:
    return uleb(len(items)) + b"".join(items)


def wasm_string(value: str) -> bytes:
    encoded = value.encode("utf-8")
    return uleb(len(encoded)) + encoded


def function_type(parameters: tuple[str, ...], result: str | None) -> bytes:
    values = {"i32": b"\x7f", "i64": b"\x7e"}
    return b"\x60" + vector([values[p] for p in parameters]) + vector([] if result is None else [values[result]])


def module(types: list[bytes], body: bytes, export: str, *, imported: bool, type_index: int = 0) -> bytes:
    sections = [(1, vector(types))]
    if imported:
        sections.append((2, vector([wasm_string("provider_ok") + wasm_string("f") + b"\x00\x00"])))
    sections.extend([
        (3, vector([uleb(type_index)])),
        (7, vector([wasm_string(export) + b"\x00" + uleb(int(imported))])),
        (10, vector([uleb(len(body) + 1) + b"\x00" + body])),
    ])
    return b"\x00asm\x01\x00\x00\x00" + b"".join(bytes([kind]) + uleb(len(data)) + data for kind, data in sections)


def write_fixtures(output: Path) -> Path:
    """Encode only these small MVP modules; also save their readable WAT equivalents."""
    fixtures = output / "wasm"
    fixtures.mkdir()
    function = function_type(("i32",), "i32")
    provider = module([function], b"\x20\x00\x41\x01\x6a\x0b", "f", imported=False)
    (fixtures / "provider_ok.wasm").write_bytes(provider)
    (fixtures / "provider_ok.wat").write_text(
        '(module (func (export "f") (param i32) (result i32)\n'
        '  local.get 0 i32.const 1 i32.add))\n'
    )
    # Function index zero is the imported add-one provider; index one is _start.
    # A wrong result executes unreachable, so exit zero proves the checked call.
    positive = module([function, function_type((), None)],
                      b"\x41\x29\x10\x00\x41\x2a\x47\x04\x40\x00\x0b\x0b",
                      "_start", imported=True, type_index=1)
    (fixtures / "consumer_linear_entry.wasm").write_bytes(positive)
    (fixtures / "consumer_linear_entry.wat").write_text(
        '(module\n  (import "provider_ok" "f" (func $f (param i32) (result i32)))\n'
        '  (func (export "_start")\n    i32.const 41 call $f i32.const 42 i32.ne\n'
        '    if unreachable end))\n'
    )
    # Same imports/signatures and call_f functions as test/0011.initializer/wat.
    # These deliberately lack _start: the required failure is import validation,
    # before entry selection. Missing entry by itself never satisfies the oracle.
    for name, params, result, _ in MISMATCHES:
        body = b"".join(b"\x20" + uleb(index) for index in range(len(params))) + b"\x10\x00\x0b"
        (fixtures / f"{name}.wasm").write_bytes(module([function_type(params, result)], body, "call_f", imported=True))
        signature = f"(param {' '.join(params)}) (result {result})"
        loads = " ".join(f"local.get {index}" for index in range(len(params)))
        (fixtures / f"{name}.wat").write_text(
            f'(module\n  (import "provider_ok" "f" (func $f {signature}))\n'
            f'  (func (export "call_f") {signature} {loads} call $f))\n'
        )
    return fixtures


def integration(args: argparse.Namespace, output: Path, binary: Path) -> list[dict[str, object]]:
    for flag in ("version", "help"):
        if execute(args, output, flag, [str(binary), f"--{flag}"], build=False):
            raise RuntimeError(f"--{flag} failed; inspect {output / (flag + '.stderr')}")
    fixtures = write_fixtures(output)
    cases = [("consumer_linear_entry", None), *((name, expected) for name, _, _, expected in MISMATCHES)]
    results = []
    for color in ("enable", "disable"):
        for name, expected in cases:
            label = f"{name}-{color}"
            command = [str(binary), "--log-color", color, "--log-verbose",
                       "--wasm-preload-library", str(fixtures / "provider_ok.wasm"), "provider_ok",
                       "--wasm-set-main-module-name", name, "--run", str(fixtures / f"{name}.wasm")]
            status = execute(args, output, label, command, build=False)
            raw = (output / f"{label}.stdout").read_bytes() + (output / f"{label}.stderr").read_bytes()
            plain = ANSI_SGR.sub(b"", raw).decode("utf-8", errors="replace")
            color_ok = bool(ANSI_SGR.search(raw)) == (color == "enable")
            if expected is None:
                behavior_ok = (status == 0 and "Begin running the WASM program." in plain
                               and "Total WASM execution time:" in plain and "[fatal]" not in plain)
            else:
                diagnostic = (f'In module "{name}", imported function "provider_ok.f" has a type mismatch. '
                              f'expected: "{expected}", got: "(i32) -> i32".')
                behavior_ok = (status not in (0, 124, 125, 126, 127, 137, -9) and diagnostic in plain
                               and "Begin running the WASM program." not in plain)
            passed = behavior_ok and color_ok
            results.append({"case": name, "color": color, "exit_status": status,
                            "behavior_ok": behavior_ok, "color_ok": color_ok, "passed": passed})
            print(f"{'PASS' if passed else 'FAIL'} {label}: exit={status}", flush=True)
    return results


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("run_dir", type=Path)
    parser.add_argument("--cpu", type=int, required=True, help="Explicit verified Linux P-core CPU number")
    parser.add_argument("--output-dir", type=Path, help="New artifact directory; must not already exist")
    parser.add_argument("--integration-only", action="store_true")
    parser.add_argument("--binary", type=Path, help="Existing executable for --integration-only; default RUN_DIR/uwvm")
    parser.add_argument("--max-virtual-bytes", type=int, default=24 * 1024**3)
    parser.add_argument("--compile-timeout", type=int, default=900, help="Per compile/link timeout in seconds")
    parser.add_argument("--run-timeout", type=int, default=30, help="Per executable invocation timeout in seconds")
    args = parser.parse_args()
    if sys.platform != "linux" or args.cpu not in os.sched_getaffinity(0):
        parser.error("Run on Linux with a CPU in the current affinity set")
    if min(args.max_virtual_bytes, args.compile_timeout, args.run_timeout) <= 0:
        parser.error("Memory and timeout limits must be positive")
    if args.binary and not args.integration_only:
        parser.error("--binary requires --integration-only")
    for tool in ("/usr/bin/time", "timeout", "taskset", "prlimit"):
        if not shutil.which(tool):
            parser.error(f"Required Linux tool is unavailable: {tool}")
    run = args.run_dir.resolve()
    command, source, main_object = read_main_run(run)
    snapshot = source.parents[3]
    # Resolve before making anything, including rejecting symlinked output paths
    # inside the source snapshot. Never reuse or overwrite earlier evidence.
    candidate = args.output_dir.resolve() if args.output_dir else run
    if candidate.is_relative_to(snapshot):
        parser.error("Artifact output must be outside the source snapshot")
    if args.output_dir:
        output = candidate
        output.mkdir(parents=True, exist_ok=False)
    else:
        output = Path(tempfile.mkdtemp(prefix="program.", dir=run))
    print(f"Artifacts: {output}", flush=True)
    report = {"schema": "uwvm-program-1", "main_run": str(run), "snapshot": str(snapshot),
              "main_command": command, "main_object_sha256": digest(main_object),
              "cpu": args.cpu, "max_virtual_bytes": args.max_virtual_bytes,
              "compile_timeout": args.compile_timeout, "run_timeout": args.run_timeout,
              "integration_only": args.integration_only, "runtime_fp_flags": list(RUNTIME_FP_FLAGS),
              "LD_LIBRARY_PATH": os.environ.get("LD_LIBRARY_PATH", ""),
              "note": "consumer_ok has no entry point and is not counted as a successful or failed execution test.",
              "passed": False}
    try:
        binary = (args.binary or run / "uwvm").resolve() if args.integration_only else build_program(
            args, output, command, source, main_object)
        if not binary.is_file() or not os.access(binary, os.X_OK):
            raise ValueError(f"Executable is unavailable: {binary}")
        report["binary"] = str(binary)
        report["binary_sha256"] = digest(binary)
        report["cases"] = integration(args, output, binary)
        report["passed"] = all(case["passed"] for case in report["cases"])
        report["wasm_sha256"] = {path.name: digest(path) for path in sorted((output / "wasm").glob("*.wasm"))}
    except (OSError, ValueError, RuntimeError) as error:
        report["error"] = str(error)
        print(str(error), file=sys.stderr)
    (output / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"{'PASS' if report['passed'] else 'FAIL'}: {output / 'results.json'}", flush=True)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        print(f"run_uwvm_program.py: {error}", file=sys.stderr)
        raise SystemExit(2)
