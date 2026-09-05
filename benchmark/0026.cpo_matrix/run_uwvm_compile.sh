#!/usr/bin/env bash
set -euo pipefail

# Reproduce one isolated, non-module uwvm2 main-TU compile. The caller supplies a frozen snapshot and an include tree;
# this script never copies from or writes into the live uwvm2 checkout. Each run receives a fresh artifact directory.
if (( $# != 3 )); then
	printf 'usage: CXX=... CPU=<verified P-core> bash %s SNAPSHOT FAST_IO_INCLUDE DISK_BUILD_DIR\n' "$0" >&2
	exit 2
fi
: "${CXX:?Supply the compiler executable}"
: "${CPU:?Supply a verified Linux P-core CPU number}"
snapshot=$(realpath "$1")
include_root=$(realpath "$2")
build_base=$(realpath "$3")
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
test -f "$snapshot/src/uwvm2/uwvm/main.default.cpp"
test -f "$include_root/fast_io.h"
test -d "$build_base"

# A per-process address-space ceiling also bounds compiler children. Reject a low-memory host before compiling;
# never apply this VM ceiling to sanitizer executables, which reserve a large, mostly uncommitted shadow mapping.
minimum_available_kib=${MINIMUM_AVAILABLE_KIB:-16777216}
available_kib=$(awk '/^MemAvailable:/ {print $2}' /proc/meminfo)
if (( available_kib < minimum_available_kib )); then
	printf 'Insufficient available memory: %s KiB\n' "$available_kib" >&2
	exit 2
fi
run_dir=$(mktemp -d "$build_base/compile.XXXXXX")
printf '%s\n' "$run_dir"
flags=(-std=c++26 -O3 -march=native -DUWVM=2
	-DUWVM_VERSION_X=2 -DUWVM_VERSION_Y=0 -DUWVM_VERSION_Z=4 -DUWVM_VERSION_S=0 -DUWVM_VERSION_DEV
	-DUWVM_DISABLE_JIT -DUWVM_USE_DEFAULT_INT -DUWVM2_USE_HUGE_FAST_IO_CPO_OUTPUT
	-I"$include_root" -I"$snapshot/third-parties/bizwen/include"
	-I"$snapshot/third-parties/boost_unordered/include" -I"$snapshot/src")
if [[ ${PROVIDERS:-0} == 1 ]]; then
	if [[ ${ALLOW_UNVERIFIED_PROVIDERS:-0} != 1 ]]; then
		printf 'Provider experiment changes the observed policy; explicit ALLOW_UNVERIFIED_PROVIDERS=1 is required.\n' >&2
		exit 2
	fi
	flags+=(-DFAST_IO_UWVM_EXPERIMENTAL_PROOFS=1)
	flags+=(-include "$script_dir/uwvm_compile_proofs.h")
fi
if [[ ${PHASE:-object} == syntax ]]; then
	flags+=(-fsyntax-only)
else
	flags+=(-c -o "$run_dir/main.o")
fi
if [[ -n ${DIAGNOSTIC:-} ]]; then
	flags+=(-DFAST_IO_SEMANTIC_CONDITION_DIAGNOSTIC="$DIAGNOSTIC")
fi
"$CXX" --version >"$run_dir/compiler.txt"
printf '%q ' "$CXX" "${flags[@]}" "$snapshot/src/uwvm2/uwvm/main.default.cpp" >"$run_dir/command.txt"
printf '\n' >>"$run_dir/command.txt"
ulimit -c 0
set +e
/usr/bin/time -v -o "$run_dir/resources.txt" timeout "${TIMEOUT_SECONDS:-240}" \
	taskset -c "$CPU" prlimit --as="${MAX_VIRTUAL_BYTES:-12884901888}" -- \
	"$CXX" "${flags[@]}" "$snapshot/src/uwvm2/uwvm/main.default.cpp" >"$run_dir/compiler.log" 2>&1
status=$?
set -e

# Some compiler crash paths have returned success after an allocation failure. A zero exit code is not sufficient
# evidence: inspect the diagnostic stream and, for object compilation, require the complete output artifact.
if (( status != 0 )) || rg -q '(^|[[:space:]])(LLVM ERROR:|fatal error:|error:)|out of memory' "$run_dir/compiler.log"; then
	printf 'Compile failed; inspect %s\n' "$run_dir" >&2
	exit 1
fi
if [[ ${PHASE:-object} != syntax ]]; then
	test -s "$run_dir/main.o"
	sha256sum "$run_dir/main.o"
	size "$run_dir/main.o"
fi
rg 'Elapsed|Maximum resident|Exit status' "$run_dir/resources.txt"
