#!/usr/bin/env bash
set -euo pipefail

export LC_ALL=C

if [[ -z "${CPU:-}" || ! "${CPU}" =~ ^[0-9]+$ ]]; then
	printf '%s\n' 'Set CPU to exactly one currently idle, permitted logical CPU.' >&2
	exit 2
fi

if ! command -v taskset >/dev/null 2>&1; then
	printf '%s\n' 'taskset is required to bind environment probes, compilation, and benchmark execution.' >&2
	exit 2
fi

if ! taskset -c "${CPU}" true 2>/dev/null; then
	printf 'CPU=%s is not an online logical CPU available to this process.\n' "${CPU}" >&2
	exit 2
fi

if [[ "$(taskset -c "${CPU}" uname -s)" != Linux ]]; then
	printf '%s\n' 'run_cpu_snapshot_linux.sh is intentionally limited to Linux.' >&2
	exit 2
fi

if ! command -v lscpu >/dev/null 2>&1; then
	printf '%s\n' 'lscpu is required to record processor topology and capabilities.' >&2
	exit 2
fi

if ! command -v realpath >/dev/null 2>&1; then
	printf '%s\n' 'realpath is required to keep every snapshot artifact below /tmp.' >&2
	exit 2
fi

compiler_request="${CXX:-c++}"
if ! compiler="$(command -v -- "${compiler_request}")"; then
	printf 'CXX=%s does not name an executable compiler.\n' "${compiler_request}" >&2
	exit 2
fi

out_dir="$(taskset -c "${CPU}" realpath -m -- \
	"${OUT_DIR:-/tmp/fast_io_prfch_cpu_snapshot_cpu${CPU}}")"
if [[ "${out_dir}" != /tmp/* ]]; then
	printf 'OUT_DIR must be an absolute directory below /tmp; got %s.\n' "${out_dir}" >&2
	exit 2
fi

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${out_dir}/build"
results_file="${out_dir}/results.csv"
run_log="${out_dir}/run.stderr.log"

taskset -c "${CPU}" mkdir -p "${out_dir}" "${build_dir}"

taskset -c "${CPU}" uname -a >"${out_dir}/uname.txt"
taskset -c "${CPU}" lscpu >"${out_dir}/lscpu.txt"
if ! taskset -c "${CPU}" lscpu -e=CPU,CORE,SOCKET,NODE,ONLINE,MAXMHZ \
	>"${out_dir}/lscpu_extended.txt" 2>/dev/null; then
	# Older util-linux releases expose the extended table but not every named column used by newer cloud images.
	taskset -c "${CPU}" lscpu -e >"${out_dir}/lscpu_extended.txt"
fi
taskset -c "${CPU}" bash -c 'taskset -pc $$' >"${out_dir}/affinity.txt"

{
	printf 'requested_CXX=%s\n' "${compiler_request}"
	printf 'resolved_CXX=%s\n' "${compiler}"
	printf 'CXXFLAGS=%s\n' "${CXXFLAGS:-<Makefile default>}"
	taskset -c "${CPU}" "${compiler}" --version
	printf '\nverbose preprocessing configuration:\n'
	taskset -c "${CPU}" "${compiler}" -march=native -v -E -x c++ /dev/null >/dev/null
} >"${out_dir}/compiler.txt" 2>&1

taskset -c "${CPU}" "${compiler}" -march=native -dM -E -x c++ /dev/null \
	>"${out_dir}/target_macros.txt"

samples="${SAMPLES:-11}"
warmups="${WARMUPS:-3}"
target_bytes="${TARGET_BYTES_PER_SAMPLE:-268435456}"
max_iterations="${MAX_ITERATIONS:-1000000}"
cold_iterations="${COLD_ITERATIONS:-1}"
cold_bytes="${COLD_BYTES:-134217728}"
seed="${SEED:-11400714819323198485}"
minimum_current_bytes="${MIN_CURRENT_BYTES:-256}"

require_positive_decimal()
{
	local name="$1"
	local value="$2"
	if [[ ! "${value}" =~ ^[0-9]+$ ]] || ((10#${value} == 0)); then
		printf '%s must be a positive decimal integer; got %s.\n' "${name}" "${value}" >&2
		exit 2
	fi
}

require_positive_decimal SAMPLES "${samples}"
# The fixture validates the destination after warm-up; at least one warm-up is part of this snapshot's preflight.
require_positive_decimal WARMUPS "${warmups}"
require_positive_decimal TARGET_BYTES_PER_SAMPLE "${target_bytes}"
require_positive_decimal MAX_ITERATIONS "${max_iterations}"
require_positive_decimal COLD_ITERATIONS "${cold_iterations}"
require_positive_decimal COLD_BYTES "${cold_bytes}"
require_positive_decimal MIN_CURRENT_BYTES "${minimum_current_bytes}"
if [[ ! "${seed}" =~ ^[0-9]+$ ]]; then
	printf 'SEED must be an unsigned decimal integer; got %s.\n' "${seed}" >&2
	exit 2
fi

{
	printf 'utc=%s\n' "$(taskset -c "${CPU}" date -u +%Y-%m-%dT%H:%M:%SZ)"
	printf 'cpu=%s\n' "${CPU}"
	printf 'compiler=%s\n' "${compiler}"
	printf 'matrix=direction:read layout:discontinuous descriptors:32 payloads:4096,16384 cache:hot,cold\n'
	printf 'samples=%s\n' "${samples}"
	printf 'warmups=%s\n' "${warmups}"
	printf 'target_bytes_per_sample=%s\n' "${target_bytes}"
	printf 'max_iterations=%s\n' "${max_iterations}"
	printf 'cold_iterations=%s\n' "${cold_iterations}"
	printf 'cold_bytes=%s\n' "${cold_bytes}"
	printf 'seed=%s\n' "${seed}"
	printf 'minimum_current_bytes=%s\n' "${minimum_current_bytes}"
} >"${out_dir}/manifest.txt"

# The matrix is deliberately fixed so snapshots from different machines retain the same operation and memory shape.
# Sample counts and byte budgets remain environment overrides: a smoke test and a retained cloud run have different
# cost envelopes, but neither may silently replace the 32-descriptor, 4/16-KiB discontinuous-read hot/cold controls.
taskset -c "${CPU}" env \
	CPU="${CPU}" \
	CXX="${compiler}" \
	BUILD_DIR="${build_dir}" \
	COUNTS='32' \
	PAYLOADS='4096 16384' \
	LAYOUTS='discontinuous' \
	CACHE_MODES='hot cold' \
	DIRECTIONS='read' \
	SAMPLES="${samples}" \
	WARMUPS="${warmups}" \
	TARGET_BYTES_PER_SAMPLE="${target_bytes}" \
	MAX_ITERATIONS="${max_iterations}" \
	COLD_ITERATIONS="${cold_iterations}" \
	COLD_BYTES="${cold_bytes}" \
	SEED="${seed}" \
	MIN_CURRENT_BYTES="${minimum_current_bytes}" \
	"${script_dir}/run_linux.sh" >"${results_file}" 2>"${run_log}"

# Reject a partial run or an accidental matrix expansion before presenting the directory as a portable snapshot.
if ! taskset -c "${CPU}" awk -F, '
	NR == 1 {
		if ($1 != "direction" || $2 != "layout" || $3 != "cache" ||
			$4 != "descriptors" || $5 != "payload_bytes")
			exit 1
		next
	}
	{
		if (NF != 15 || $1 != "read" || $2 != "discontinuous" || $4 != 32 ||
			($3 != "hot" && $3 != "cold") || ($5 != 4096 && $5 != 16384))
			exit 1
		++seen[$3 ":" $5]
	}
	END {
		if (NR != 5 || seen["hot:4096"] != 1 || seen["hot:16384"] != 1 ||
			seen["cold:4096"] != 1 || seen["cold:16384"] != 1)
			exit 1
	}
' "${results_file}"; then
	printf 'Snapshot CSV failed the fixed four-case matrix check: %s\n' "${results_file}" >&2
	exit 1
fi

printf 'CPU snapshot complete: %s\n' "${out_dir}"
