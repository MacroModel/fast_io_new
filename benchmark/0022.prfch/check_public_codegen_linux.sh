#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != Linux ]]; then
	printf '%s\n' 'check_public_codegen_linux.sh is intentionally limited to Linux.' >&2
	exit 2
fi

if [[ -z "${CPU:-}" || ! "${CPU}" =~ ^[0-9]+$ ]]; then
	printf '%s\n' 'Set CPU to one currently idle permitted logical CPU.' >&2
	exit 2
fi

if ! command -v taskset >/dev/null 2>&1; then
	printf '%s\n' 'taskset is required to bind every compiler process.' >&2
	exit 2
fi

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/../.." && pwd)"
build_dir="${BUILD_DIR:-/tmp/fast_io_prfch_public_codegen}"
compiler="${CXX:-g++}"
source_file="${script_dir}/public_codegen_probe.cc"

mkdir -p "${build_dir}"

if ! taskset -c "${CPU}" "${compiler}" --version 2>/dev/null | head -n 1 | grep -q 'g++\|GCC\|gcc'; then
	printf '%s\n' 'This regression check requires genuine GCC.' >&2
	exit 2
fi

target_macros="$({ printf '\n'; } | taskset -c "${CPU}" "${compiler}" -march=native -dM -E -x c++ -)"
if ! grep -q '^#define __GNUC__ 15$' <<<"${target_macros}"; then
	printf '%s\n' 'The exact instruction-count contract is currently retained only for genuine GCC 15.' >&2
	exit 2
fi
if ! grep -Eq '^#define __tune_(alderlake|arrowlake|arrowlake_s|pantherlake|novalake)__ ' <<<"${target_macros}"; then
	printf '%s\n' 'The native GCC target is not classified as x86_intel_hybrid; refusing a misleading positive check.' >&2
	exit 2
fi

compile_probe()
{
	local site="$1"
	local proved="$2"
	local output="$3"
	taskset -c "${CPU}" "${compiler}" -std=c++23 -O3 -march=native -fno-prefetch-loop-arrays \
		-Wall -Wextra -Wpedantic -Werror \
		-I"${repo_root}/include" -DFAST_IO_DISABLE_FLOATING_POINT \
		-DFAST_IO_PRFCH_PUBLIC_PROBE_SITE="${site}" \
		-DFAST_IO_PRFCH_PUBLIC_PROBE_PROVED="${proved}" \
		-S "${source_file}" -o "${output}"
}

count_l1_read_hints()
{
	awk '$1 == "prefetcht0" { ++count } END { print count + 0 }' "$1"
}

require_hint_count()
{
	local file="$1"
	local expected="$2"
	local label="$3"
	local actual
	actual="$(count_l1_read_hints "${file}")"
	if [[ "${actual}" != "${expected}" ]]; then
		printf 'FAIL %s: expected %s prefetcht0 instructions, found %s in %s\n' \
			"${label}" "${expected}" "${actual}" "${file}" >&2
		exit 1
	fi
	printf 'PASS %s: prefetcht0=%s\n' "${label}" "${actual}"
}

concat_proved="${build_dir}/concat_proved.s"
concat_unproved="${build_dir}/concat_unproved.s"
print_proved="${build_dir}/print_proved.s"
print_unproved="${build_dir}/print_unproved.s"

compile_probe 1 1 "${concat_proved}"
compile_probe 1 0 "${concat_unproved}"
compile_probe 2 1 "${print_proved}"
compile_probe 2 0 "${print_unproved}"

# Concat retains one explicit instruction in its next-nonempty loop. Print's variadic materializer is intentionally
# unrolled and therefore has one static instruction for each of the 31 edges between 32 nonempty sources.
require_hint_count "${concat_proved}" 1 'public concat proved'
require_hint_count "${concat_unproved}" 0 'public concat unproved'
require_hint_count "${print_proved}" 31 'public print proved'
require_hint_count "${print_unproved}" 0 'public print unproved'
