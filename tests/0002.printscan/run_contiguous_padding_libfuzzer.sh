#!/usr/bin/env bash

# This is the only supported entry point for the contiguous-padding libFuzzer
# target.  Normal engine, sanitizer, compiler, and worker output is discarded.
# The script emits exactly one final PASS/FAIL line.  On failure it replays the
# artifact once, still silently, and reports only whether replay reproduced it.

set -u
set -o pipefail

readonly script_dir="$(
	cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1
	pwd
)"
readonly repository_root="$(
	cd -- "${script_dir}/../.." >/dev/null 2>&1
	pwd
)"
readonly source_file="${script_dir}/contiguous_padding_differential_fuzz.cc"
readonly configuration_count=2050
readonly integer_configuration_count=1750
readonly default_local_runs_per_configuration=5000
readonly default_linux_runs_per_configuration=2000

work_directory=
preserve_work=false
linux_replay_cpu=

finish()
{
	local status="$1"
	local message="$2"
	if [[ "${status}" -ne 0 ]]
	then
		preserve_work=true
	fi
	if [[ "${preserve_work}" != true && -n "${work_directory}" ]]
	then
		rm -rf -- "${work_directory}"
	fi
	printf '%s\n' "${message}"
	exit "${status}"
}

choose_compiler()
{
	if [[ -n "${FAST_IO_LIBFUZZER_CXX:-}" ]]
	then
		printf '%s\n' "${FAST_IO_LIBFUZZER_CXX}"
		return
	fi
	if [[ "$(uname -s)" == Darwin &&
		  -x /opt/homebrew/opt/llvm/bin/clang++ ]]
	then
		printf '%s\n' /opt/homebrew/opt/llvm/bin/clang++
		return
	fi
	local version
	for version in 22 21 20 19 18 17
	do
		if command -v "clang++-${version}" >/dev/null 2>&1
		then
			command -v "clang++-${version}"
			return
		fi
	done
	if command -v clang++ >/dev/null 2>&1
	then
		command -v clang++
		return
	fi
	return 1
}

prepare_work_directory()
{
	if [[ -n "${FAST_IO_LIBFUZZER_WORK_DIR:-}" ]]
	then
		work_directory="${FAST_IO_LIBFUZZER_WORK_DIR}"
		mkdir -p -- "${work_directory}" || return 1
		preserve_work=true
	else
		work_directory="$(
			mktemp -d "${TMPDIR:-/tmp}/fast_io_padding_libfuzzer.XXXXXX"
		)" || return 1
	fi
	mkdir -p -- "${work_directory}/artifacts" || return 1
}

build_target()
{
	local compiler
	compiler="$(choose_compiler)" || return 1
	: >"${work_directory}/compile.log"
	local character_index
	for ((character_index = 0; character_index != 5; ++character_index))
	do
		"${compiler}" \
			-std=c++20 -O2 -gline-tables-only -march=native \
			-fno-omit-frame-pointer \
			-Wno-invalid-constexpr \
			-fsanitize=fuzzer,address,undefined \
			"-DFAST_IO_PADDING_FUZZ_CHAR_INDEX=${character_index}" \
			-I"${repository_root}/include" \
			"${source_file}" \
			-o "${work_directory}/contiguous_padding_libfuzzer-${character_index}" \
			>>"${work_directory}/compile.log" 2>&1 ||
			return 1
	done
}

generate_fixed_corpora()
{
	local destination="$1"
	mkdir -p -- "${destination}" || return 1
	python3 - "${destination}" <<'PY' \
		>"${work_directory}/corpus.log" 2>&1
import pathlib
import struct
import sys

destination = pathlib.Path(sys.argv[1])

integer_configuration_count = 5 * 10 * 35
configuration_count = integer_configuration_count + 5 * 6 * 10
integer_boundary_count = 24
floating_boundary_count = 38

for case_index in range(configuration_count):
    case_directory = destination / f"case-{case_index:04d}"
    case_directory.mkdir(exist_ok=True)
    boundary_count = (
        integer_boundary_count
        if case_index < integer_configuration_count
        else floating_boundary_count
    )
    for boundary_index in range(boundary_count):
        # The first ten bytes are the configuration and abstract-iteration
        # selectors.  The final eight bytes make semantically equal boundary
        # seeds produce distinct spellings and padding values.
        payload = struct.pack(
            "<HQ", case_index, boundary_index
        ) + struct.pack(
            "<Q",
            (case_index * 0x9E3779B97F4A7C15 + boundary_index)
            & ((1 << 64) - 1),
        )
        path = case_directory / f"boundary-{boundary_index:02d}"
        path.write_bytes(payload)
PY
}

configuration_character_index()
{
	local configuration="$1"
	if ((configuration < integer_configuration_count))
	then
		printf '%s\n' "$((configuration / 350))"
	else
		printf '%s\n' "$(((configuration - integer_configuration_count) / 60))"
	fi
}

run_one()
{
	local executable="$1"
	local configuration="$2"
	local corpus="$3"
	local artifact_directory="$4"
	local runs="$5"
	shift 5
	mkdir -p -- "${artifact_directory}" || return 1
	ASAN_OPTIONS=abort_on_error=1:detect_leaks=1:allocator_may_return_null=0 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=0 \
	FAST_IO_PADDING_FIXED_CASE="${configuration}" \
		"$@" "${executable}" \
		"${corpus}" \
		-runs="${runs}" \
		-max_len=64 \
		-timeout=30 \
		-rss_limit_mb=6144 \
		-detect_leaks=1 \
		-print_final_stats=0 \
		-verbosity=0 \
		-artifact_prefix="${artifact_directory}/" \
		>/dev/null 2>&1
}

newest_artifact()
{
	find "${work_directory}/artifacts" -type f \
		\( -name 'crash-*' -o -name 'timeout-*' -o \
		   -name 'leak-*' -o -name 'oom-*' -o -name 'slow-unit-*' \) \
		-exec ls -1t {} + 2>/dev/null |
		head -n 1
}

replay_artifact()
{
	local artifact="$1"
	local case_name
	case_name="$(basename -- "$(dirname -- "${artifact}")")"
	local configuration="${case_name#case-}"
	configuration="$((10#${configuration}))"
	local character_index
	character_index="$(configuration_character_index "${configuration}")"
	local command_prefix=()
	if [[ -n "${linux_replay_cpu}" ]]
	then
		command_prefix=(taskset -c "${linux_replay_cpu}")
	fi
	ASAN_OPTIONS=abort_on_error=1:detect_leaks=1:allocator_may_return_null=0 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=0 \
	FAST_IO_PADDING_FIXED_CASE="${configuration}" \
		"${command_prefix[@]}" \
		"${work_directory}/contiguous_padding_libfuzzer-${character_index}" \
		"${artifact}" \
		-runs=1 \
		-print_final_stats=0 \
		-verbosity=0 \
		>/dev/null 2>&1
}

report_failure()
{
	local platform="$1"
	local artifact
	artifact="$(newest_artifact)"
	preserve_work=true
	if [[ -z "${artifact}" ]]
	then
		finish 1 \
			"FAIL platform=${platform} artifact=none work=${work_directory}"
	fi
	if replay_artifact "${artifact}"
	then
		finish 1 \
			"FAIL platform=${platform} replay=not-reproduced artifact=${artifact}"
	fi
	finish 1 \
		"FAIL platform=${platform} replay=reproduced artifact=${artifact}"
}

detect_p_cores()
{
	if [[ -n "${FAST_IO_PCORE_CPUS:-}" ]]
	then
		printf '%s\n' "${FAST_IO_PCORE_CPUS}" |
			tr ',' '\n'
		return
	fi
	local topology cpu siblings first
	local count=0
	for topology in /sys/devices/system/cpu/cpu[0-9]*/topology
	do
		[[ -r "${topology}/thread_siblings_list" ]] || continue
		cpu="${topology%/topology}"
		cpu="${cpu##*cpu}"
		siblings="$(<"${topology}/thread_siblings_list")"
		# Select one logical CPU from each SMT pair.  On the target hybrid
		# machine this excludes single-threaded E-cores and prevents siblings
		# from sharing one physical P-core.
		if [[ "${siblings}" == *","* || "${siblings}" == *"-"* ]]
		then
			first="${siblings%%,*}"
			first="${first%%-*}"
			if [[ "${cpu}" == "${first}" ]]
			then
				printf '%s\n' "${cpu}"
				((++count == 6)) && return
			fi
		fi
	done
}

run_local()
{
	prepare_work_directory ||
		finish 1 "FAIL platform=local stage=work-directory"
	build_target ||
		finish 1 "FAIL platform=local stage=compile log=${work_directory}/compile.log"
	generate_fixed_corpora "${work_directory}/corpus" ||
		finish 1 "FAIL platform=local stage=corpus log=${work_directory}/corpus.log"
	local runs="${FAST_IO_LIBFUZZER_RUNS:-${default_local_runs_per_configuration}}"
	local configuration character_index
	for ((configuration = 0; configuration != configuration_count;
		 ++configuration))
	do
		character_index="$(
			configuration_character_index "${configuration}"
		)"
		if ! run_one \
			"${work_directory}/contiguous_padding_libfuzzer-${character_index}" \
			"${configuration}" \
			"${work_directory}/corpus/case-$(printf '%04d' "${configuration}")" \
			"${work_directory}/artifacts/case-$(printf '%04d' "${configuration}")" \
			"${runs}"
		then
			report_failure local-single-thread
		fi
	done
	finish 0 \
		"PASS platform=local-single-thread processes=${configuration_count} runs-per-process=${runs}"
}

run_fixed_configuration()
{
	local configuration="$1"
	local cpu="$2"
	local runs="$3"
	local character_index
	character_index="$(configuration_character_index "${configuration}")"
	local case_name
	case_name="$(printf 'case-%04d' "${configuration}")"
	run_one \
		"${work_directory}/contiguous_padding_libfuzzer-${character_index}" \
		"${configuration}" \
		"${work_directory}/corpus/${case_name}" \
		"${work_directory}/artifacts/${case_name}" \
		"${runs}" taskset -c "${cpu}"
}

run_linux_p_cores()
{
	prepare_work_directory ||
		finish 1 "FAIL platform=linux stage=work-directory"
	build_target ||
		finish 1 "FAIL platform=linux stage=compile log=${work_directory}/compile.log"
	local cpus=()
	while IFS= read -r cpu
	do
		[[ -n "${cpu}" ]] && cpus+=("${cpu}")
	done < <(detect_p_cores)
	if [[ "${#cpus[@]}" -eq 0 ]]
	then
		finish 1 "FAIL platform=linux stage=p-core-detection"
	fi
	linux_replay_cpu="${cpus[0]}"
	local workers="${#cpus[@]}"
	local runs="${FAST_IO_LIBFUZZER_RUNS:-${default_linux_runs_per_configuration}}"
	generate_fixed_corpora "${work_directory}/corpus" ||
		finish 1 \
			"FAIL platform=linux stage=corpus log=${work_directory}/corpus.log"
	local pids=()
	local failed=false
	local configuration slot
	for ((configuration = 0; configuration != configuration_count;
		 ++configuration))
	do
		slot="$((configuration % workers))"
		if [[ -n "${pids[slot]:-}" ]]
		then
			if ! wait "${pids[slot]}"
			then
				failed=true
			fi
		fi
		run_fixed_configuration \
			"${configuration}" "${cpus[slot]}" "${runs}" &
		pids[slot]="$!"
	done
	local pid
	for pid in "${pids[@]}"
	do
		if ! wait "${pid}"
		then
			failed=true
		fi
	done
	if [[ "${failed}" == true ]]
	then
		report_failure linux-p-cores
	fi
	finish 0 \
		"PASS platform=linux-p-cores workers=${workers} processes=${configuration_count} runs-per-process=${runs}"
}

run_remote()
{
	local host="${1:-}"
	[[ -n "${host}" ]] ||
		finish 2 "FAIL platform=remote stage=missing-host"
	local remote_directory
	remote_directory="$(
		ssh "${host}" 'mktemp -d /tmp/fast_io_padding_libfuzzer.XXXXXX'
	)" || finish 1 "FAIL platform=remote stage=work-directory"
	ssh "${host}" \
		"mkdir -p '${remote_directory}/tests/0002.printscan'" \
		>/dev/null 2>&1 ||
		finish 1 "FAIL platform=remote stage=prepare"
	rsync -a -- "${repository_root}/include" \
		"${host}:${remote_directory}/" >/dev/null 2>&1 ||
		finish 1 "FAIL platform=remote stage=sync-include"
	rsync -a -- "${source_file}" "$0" \
		"${host}:${remote_directory}/tests/0002.printscan/" \
		>/dev/null 2>&1 ||
		finish 1 "FAIL platform=remote stage=sync-target"
	local result_file="${remote_directory}/result.txt"
	if ssh "${host}" \
		"cd '${remote_directory}' && FAST_IO_LIBFUZZER_WORK_DIR='${remote_directory}/run' FAST_IO_LIBFUZZER_RUNS='${FAST_IO_LIBFUZZER_RUNS:-${default_linux_runs_per_configuration}}' ./tests/0002.printscan/run_contiguous_padding_libfuzzer.sh linux-p-core >'${result_file}'" \
		>/dev/null 2>&1
	then
		local result
		result="$(ssh "${host}" "cat '${result_file}'" 2>/dev/null)"
		ssh "${host}" "rm -rf -- '${remote_directory}'" >/dev/null 2>&1
		printf '%s\n' "${result}"
		exit 0
	fi
	local local_artifacts
	local_artifacts="$(
		mktemp -d \
			"${repository_root}/contiguous-padding-libfuzzer-artifacts.XXXXXX"
	)" || finish 1 "FAIL platform=remote stage=local-artifact-directory"
	rsync -a -- "${host}:${remote_directory}/run/artifacts/" \
		"${local_artifacts}/" >/dev/null 2>&1
	local result
	result="$(ssh "${host}" "cat '${result_file}'" 2>/dev/null)"
	printf '%s local-artifacts=%s remote-work=%s\n' \
		"${result:-FAIL platform=remote}" \
		"${local_artifacts}" "${remote_directory}"
	exit 1
}

case "${1:-local}" in
local)
	run_local
	;;
linux-p-core)
	run_linux_p_cores
	;;
remote)
	run_remote "${2:-}"
	;;
*)
	printf '%s\n' \
		"FAIL stage=usage expected='local|linux-p-core|remote HOST'"
	exit 2
	;;
esac
