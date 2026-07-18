#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != Linux ]]; then
	printf '%s\n' 'run_linux.sh is intentionally limited to Linux.' >&2
	exit 2
fi

if [[ -z "${CPU:-}" ]]; then
	printf '%s\n' 'Set CPU to one currently idle, permitted logical CPU and leave its SMT sibling unused.' >&2
	exit 2
fi

if [[ ! "${CPU}" =~ ^[0-9]+$ ]]; then
	printf '%s\n' 'CPU must name exactly one logical CPU.' >&2
	exit 2
fi

if ! command -v taskset >/dev/null 2>&1; then
	printf '%s\n' 'taskset is required to preserve the selected-core benchmark contract.' >&2
	exit 2
fi

if ! taskset -c "${CPU}" true 2>/dev/null; then
	printf 'CPU=%s is not an online logical CPU available to this process.\n' "${CPU}" >&2
	exit 2
fi

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/../.." && pwd)"
build_dir="${BUILD_DIR:-/tmp/fast_io_prfch_bench}"
binary="${build_dir}/scatter_chain_bench"
compiler="${CXX:-c++}"

mkdir -p "${build_dir}"
taskset -c "${CPU}" make -C "${script_dir}" -j1 \
	ROOT="${repo_root}" BUILD_DIR="${build_dir}" OUT="${binary}" CXX="${compiler}" \
	MIN_CURRENT_BYTES="${MIN_CURRENT_BYTES:-256}" >&2

counts="${COUNTS:-2 8 32 128 1024}"
payloads="${PAYLOADS:-16 64 256 512 4096}"
layouts="${LAYOUTS:-contiguous discontinuous}"
cache_modes="${CACHE_MODES:-hot cold}"
directions="${DIRECTIONS:-read write}"
samples="${SAMPLES:-9}"
warmups="${WARMUPS:-2}"
target_bytes="${TARGET_BYTES_PER_SAMPLE:-67108864}"
max_iterations="${MAX_ITERATIONS:-1000000}"
cold_iterations="${COLD_ITERATIONS:-1}"
cold_bytes="${COLD_BYTES:-67108864}"
seed="${SEED:-11400714819323198485}"

runner=(taskset -c "${CPU}" "${binary}")
"${runner[@]}" --header

for direction in ${directions}; do
	for layout in ${layouts}; do
		for cache_mode in ${cache_modes}; do
			for count in ${counts}; do
				for payload in ${payloads}; do
					bytes=$((count * payload))
					if [[ "${cache_mode}" == hot ]]; then
						iterations=$((target_bytes / bytes))
						if ((iterations < 1)); then
							iterations=1
						elif ((iterations > max_iterations)); then
							iterations=${max_iterations}
						fi
					else
						iterations=${cold_iterations}
					fi

					"${runner[@]}" \
						--direction "${direction}" \
						--layout "${layout}" \
						--cache "${cache_mode}" \
						--descriptors "${count}" \
						--payload "${payload}" \
						--iterations "${iterations}" \
						--samples "${samples}" \
						--warmups "${warmups}" \
						--cold-bytes "${cold_bytes}" \
						--seed "${seed}"
				done
			done
		done
	done
done
