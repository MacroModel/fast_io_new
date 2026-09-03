#!/usr/bin/env bash

set -u
set -o pipefail

# This runner is intentionally Linux-only: each compiler cell is pinned to one
# explicitly supplied E-core, preventing correctness stress from perturbing the
# P-core benchmark environment. The default list matches the 16 E-cores of the
# validation host; callers must override it when topology differs.
root=${FAST_IO_RANDOM_ROOT:-$(cd "$(dirname "$0")/../.." && pwd)}
build_root=${FAST_IO_RANDOM_BUILD_ROOT:-/tmp/fast_io_unsigned_decimal_random}
cases_per_width=${FAST_IO_RANDOM_CASES_PER_WIDTH:-65536}
seed=${FAST_IO_RANDOM_SEED:-0x243f6a8885a308d3}
sanitizers=${FAST_IO_RANDOM_SANITIZERS:-0}
ecore_text=${FAST_IO_ECORE_LIST:-16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31}
source_file=$root/tests/0002.printscan/unsigned_decimal_specialized_random.cc
edge_source_file=$root/tests/0002.printscan/unsigned_decimal_specialized_edges.cc

IFS=',' read -r -a ecores <<< "$ecore_text"
if ((${#ecores[@]} != 16)); then
	printf 'FAST_IO_ECORE_LIST must name exactly 16 logical E-cores\n' >&2
	exit 2
fi
for core in "${ecores[@]}"; do
	if [[ ! $core =~ ^[0-9]+$ ]] || [[ ! -d /sys/devices/system/cpu/cpu$core ]]; then
		printf 'invalid E-core id: %s\n' "$core" >&2
		exit 2
	fi
done

compilers=()
labels=()
for version in 11 12 13 14 15 16; do
	if command -v "g++-$version" >/dev/null 2>&1; then
		compilers+=("$(command -v "g++-$version")")
		labels+=("gcc-$version")
	fi
done
for version in 17 18 19 20 21 22 23; do
	if command -v "clang++-$version" >/dev/null 2>&1; then
		compilers+=("$(command -v "clang++-$version")")
		labels+=("clang-$version")
	fi
done
if ((${#compilers[@]} == 0)); then
	printf 'no versioned GCC or Clang compiler was found\n' >&2
	exit 2
fi

mode=release
common_flags=(-std=c++20 -I"$root/include" -march=native
	-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror)
mode_flags=(-O3)
if ((sanitizers != 0)); then
	mode=sanitize
	mode_flags=(-O1 -g -fno-omit-frame-pointer -fsanitize=address,leak,undefined)
fi
mode_root=$build_root/$mode
mkdir -p "$mode_root"

pids=()
job_labels=()
for index in "${!compilers[@]}"; do
	compiler=${compilers[index]}
	label=${labels[index]}
	core=${ecores[index % ${#ecores[@]}]}
	binary=$mode_root/$label
	edge_binary=$mode_root/$label-edges
	log=$mode_root/$label.log
	extra_flags=()
	if [[ $label == clang-* ]]; then
		extra_flags+=(-Wno-pass-failed)
		# Clang 17 and 18 cannot parse GCC 16 libstdc++ headers that use newer
		# GCC-only builtins. Selecting the newest compatible installed GCC keeps
		# the standard library/compiler pair within its supported language surface.
		clang_version=${label#clang-}
		if ((clang_version <= 18)) && [[ -d /usr/lib/gcc/x86_64-linux-gnu/14 ]]; then
			extra_flags+=(--gcc-install-dir=/usr/lib/gcc/x86_64-linux-gnu/14)
		fi
	fi
	(
		set -e
		taskset -c "$core" "$compiler" "${common_flags[@]}" \
			"${mode_flags[@]}" "${extra_flags[@]}" "$edge_source_file" \
			-o "$edge_binary"
		taskset -c "$core" "$compiler" "${common_flags[@]}" \
			"${mode_flags[@]}" "${extra_flags[@]}" "$source_file" -o "$binary"
		if ((sanitizers != 0)); then
			ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:strict_string_checks=1 \
			LSAN_OPTIONS=exitcode=101 \
			UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
				taskset -c "$core" "$edge_binary"
			ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:strict_string_checks=1 \
			LSAN_OPTIONS=exitcode=101 \
			UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
				taskset -c "$core" "$binary" "$cases_per_width" "$seed"
		else
			taskset -c "$core" "$edge_binary"
			taskset -c "$core" "$binary" "$cases_per_width" "$seed"
		fi
	) >"$log" 2>&1 &
	pids+=("$!")
	job_labels+=("$label@cpu$core")
done

status=0
for index in "${!pids[@]}"; do
	if wait "${pids[index]}"; then
		printf 'PASS %-16s %s\n' "${job_labels[index]}" \
			"$(tail -n 1 "$mode_root/${labels[index]}.log")"
	else
		cell_status=$?
		printf 'FAIL %-16s status=%d log=%s\n' "${job_labels[index]}" \
			"$cell_status" "$mode_root/${labels[index]}.log" >&2
		sed -n '1,160p' "$mode_root/${labels[index]}.log" >&2
		status=1
	fi
done
exit "$status"
