#!/usr/bin/env bash
#
# Full precise-size differential campaign for Linux/x86-64.
#
# The policy target compares print_reserve_precise_size/define with the
# ordinary reserve writer, exact-fit to_chars, and all five supported character
# widths.  A single fuzzer input exercises all ten deterministic rounding
# policies, all four decimal presentations, and both JSON states.  Shards are:
#   0 shortest; 1 significant; 2 fractional; 3 significant-preserve;
#   4 fractional-preserve; 5..8 precision_range for the same four modes;
#   9 exact_decimal.
#
# The layout target independently compares the new exact-decimal metadata
# path with the original limb layout.  Its seventh domain is the synthetic
# IBM double-double dyadic carrier, which is necessary on x86 where native
# long double is binary80 rather than IBM double-double.
#
# Every fuzz invocation runs one million inputs by default.  Override only
# for a smoke run, for example:
#   FAST_IO_PRECISE_FUZZ_RUNS=1000 bash tests/0004.floating/run_precise_size_full_fuzz_linux.sh
#
# All compiler and libFuzzer output is redirected to per-job log files.  The
# script writes no progress output; it prints one final summary line only.

set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
root_dir=$(cd -- "${script_dir}/../.." && pwd)
lock_file="/tmp/fast_io-precise-size-full-fuzz.${UID}.lock"
exec {lock_fd}>"${lock_file}"
if ! flock -n "${lock_fd}"; then
	printf 'FAILED: another precise-size fuzz campaign is already running\n'
	exit 3
fi
build_dir=$(mktemp -d /tmp/fast_io-precise-size-full-fuzz.XXXXXX)
log_root="${FAST_IO_PRECISE_FUZZ_LOG_DIR:-${build_dir}/logs}"
run_stamp=$(date +%Y%m%d-%H%M%S)-$$
log_dir="${log_root}/${run_stamp}"
artifact_dir="${build_dir}/artifacts"
summary_file="${log_dir}/summary.tsv"

cxx=${CXX:-clang++-21}
runs=${FAST_IO_PRECISE_FUZZ_RUNS:-1000000}
max_len=${FAST_IO_PRECISE_FUZZ_MAX_LEN:-24}
build_only=${FAST_IO_PRECISE_FUZZ_BUILD_ONLY:-0}

fuzz_cores=(16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 0 2 4 6)
p_cores=(0 2 4 6 8 10 12 14)

mkdir -p "${log_dir}" "${artifact_dir}"
ln -sfn "${log_dir}" "${log_root}/latest"
: > "${summary_file}"

if ! command -v "${cxx}" >/dev/null 2>&1; then
	printf 'FAILED: compiler %s is unavailable; logs=%s\n' "${cxx}" "${log_dir}"
	exit 2
fi

sanitize_flags=(
	-std=c++20 -O1 -g -fno-omit-frame-pointer
	-fsanitize=fuzzer,address,leak,undefined
	-I"${root_dir}/include"
)
constexpr_flags=(
	-std=c++20 -O2
	-I"${root_dir}/include"
)

declare -a build_pids=()
declare -a build_labels=()
declare -a fuzz_pids=()
declare -a fuzz_labels=()

terminate_children() {
	local pid
	for pid in "${build_pids[@]}" "${fuzz_pids[@]}"; do
		kill "${pid}" 2>/dev/null || true
	done
	for pid in "${build_pids[@]}" "${fuzz_pids[@]}"; do
		wait "${pid}" 2>/dev/null || true
	done
}

on_signal() {
	local signal=$1
	trap - TERM INT HUP
	terminate_children
	printf 'INTERRUPTED: signal=%s logs=%s summary=%s\n' \
		"${signal}" "${log_dir}" "${summary_file}" >&2
	exit 128
}

trap 'on_signal TERM' TERM
trap 'on_signal INT' INT
trap 'on_signal HUP' HUP

build_failed=0
fuzz_failed=0
build_index=0
fuzz_index=0
seed=1

wait_build_batch() {
	local index
	for index in "${!build_pids[@]}"; do
		if wait "${build_pids[index]}"; then
			printf 'build\t%s\tpass\n' "${build_labels[index]}" >> "${summary_file}"
		else
			printf 'build\t%s\tfail\tstatus=%s\n' \
				"${build_labels[index]}" "$?" >> "${summary_file}"
			build_failed=1
		fi
	done
	build_pids=()
	build_labels=()
}

enqueue_build() {
	local label=$1
	local output=$2
	shift 2
	local core=${p_cores[build_index % ${#p_cores[@]}]}
	build_index=$((build_index + 1))
	(
		taskset -c "${core}" "${cxx}" "$@" -o "${output}"
	) > "${log_dir}/${label}.build.log" 2>&1 &
	build_pids+=("$!")
	build_labels+=("${label}")
	if ((${#build_pids[@]} == ${#p_cores[@]})); then
		wait_build_batch
	fi
}

wait_fuzz_batch() {
	local index
	for index in "${!fuzz_pids[@]}"; do
		if wait "${fuzz_pids[index]}"; then
			printf 'fuzz\t%s\tpass\n' "${fuzz_labels[index]}" >> "${summary_file}"
		else
			printf 'fuzz\t%s\tfail\tstatus=%s\n' \
				"${fuzz_labels[index]}" "$?" >> "${summary_file}"
			fuzz_failed=1
		fi
	done
	fuzz_pids=()
	fuzz_labels=()
}

enqueue_fuzz() {
	local label=$1
	local binary=$2
	shift 2
	local core=${fuzz_cores[fuzz_index % ${#fuzz_cores[@]}]}
	local this_seed=${seed}
	fuzz_index=$((fuzz_index + 1))
	seed=$((seed + 1))
	(
		ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1:symbolize=0 \
		LSAN_OPTIONS=exitcode=23:report_objects=0:verbosity=0 \
		UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=0 \
		taskset -c "${core}" "$@" "${binary}" \
			-runs="${runs}" -max_len="${max_len}" -verbosity=0 \
			-seed="${this_seed}" -artifact_prefix="${artifact_dir}/"
	) > "${log_dir}/${label}.fuzz.log" 2>&1 &
	fuzz_pids+=("$!")
	fuzz_labels+=("${label}")
	if ((${#fuzz_pids[@]} == ${#fuzz_cores[@]})); then
		wait_fuzz_batch
	fi
}

for source in exact_decimal_constexpr precision_range_constexpr precision_range; do
	enqueue_build "constexpr-${source}" "${build_dir}/${source}" \
		"${constexpr_flags[@]}" \
		"${root_dir}/tests/0004.floating/${source}.cc"
done

enqueue_build layout-oracle "${build_dir}/layout-oracle" \
	"${sanitize_flags[@]}" \
	"${root_dir}/tests/0004.floating/exact_decimal_precise_size_differential_fuzz.cc"

for domain in 0 1 2 3 4 5; do
	for shard in 0 1 2 3 4 5 6 7 8 9; do
		enqueue_build "policy-d${domain}-s${shard}" \
			"${build_dir}/policy-d${domain}-s${shard}" \
			"${sanitize_flags[@]}" \
			-DFAST_IO_PRECISE_POLICY_FUZZ_DOMAIN="${domain}" \
			-DFAST_IO_PRECISE_POLICY_FUZZ_SHARD="${shard}" \
			"${root_dir}/tests/0004.floating/floating_precise_size_policy_matrix_fuzz.cc"
	done
done

wait_build_batch
if ((build_failed)); then
	printf 'FAILED: build errors; logs=%s summary=%s\n' "${log_dir}" "${summary_file}"
	exit 1
fi

if [[ ${build_only} == 1 ]]; then
	printf 'PASS: all 64 build targets; logs=%s summary=%s\n' \
		"${log_dir}" "${summary_file}"
	exit 0
fi

for source in exact_decimal_constexpr precision_range_constexpr precision_range; do
	if taskset -c 4 "${build_dir}/${source}" \
		> "${log_dir}/constexpr-${source}.run.log" 2>&1; then
		printf 'constexpr\t%s\tpass\n' "${source}" >> "${summary_file}"
	else
		printf 'constexpr\t%s\tfail\n' "${source}" >> "${summary_file}"
		printf 'FAILED: constexpr test; logs=%s summary=%s\n' "${log_dir}" "${summary_file}"
		exit 1
	fi
done

# bf16, f16, f32, f64, f80, f128, synthetic IBM double-double.
for domain in 0 1 2 3 4 5 6; do
	enqueue_fuzz "layout-d${domain}" "${build_dir}/layout-oracle" \
		env FAST_IO_PRECISE_SIZE_FUZZ_DOMAIN="${domain}"
done

# Native IBM double-double requires a PowerPC IBM-long-double target.  The
# synthetic dyadic layout campaign above covers its 106-significand-bit exact
# decimal metadata on this x86 worker; the six native domains below exercise
# the full ordinary-versus-precise output protocol.
for domain in 0 1 2 3 4 5; do
	for shard in 0 1 2 3 4 5 6 7 8 9; do
		enqueue_fuzz "policy-d${domain}-s${shard}" \
			"${build_dir}/policy-d${domain}-s${shard}"
	done
done

wait_fuzz_batch
if ((fuzz_failed)); then
	printf 'FAILED: fuzz failure; logs=%s artifacts=%s summary=%s\n' \
		"${log_dir}" "${artifact_dir}" "${summary_file}"
	exit 1
fi

printf 'PASS: complete; logs=%s artifacts=%s summary=%s\n' \
	"${log_dir}" "${artifact_dir}" "${summary_file}"
