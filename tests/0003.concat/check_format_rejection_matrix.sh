#!/usr/bin/env bash
set -euo pipefail

root=${ROOT:-$(cd "$(dirname "$0")/../.." && pwd)}
test_dir="$root/tests/0003.concat"
work_dir=$(mktemp -d)
trap 'find "$work_dir" -type f -delete; rmdir "$work_dir"' EXIT

common_flags=(-std=c++20 -O3 -fno-exceptions -fno-rtti -fsyntax-only \
	-I"$root/include")

declare -a compilers=()
declare -A seen=()
add_compiler()
{
	local candidate=$1
	command -v "$candidate" >/dev/null 2>&1 || return 0
	local path identity
	path=$(command -v "$candidate")
	identity=$(readlink -f "$path" 2>/dev/null || printf '%s' "$path")
	if [[ -z ${seen[$identity]+x} ]]; then
		seen[$identity]=1
		compilers+=("$path")
	fi
}

for version in 13 14 15 16; do
	variable="GCC${version}_CXX"
	[[ -z ${!variable:-} ]] || add_compiler "${!variable}"
	add_compiler "g++-$version"
done
add_compiler g++
for version in 17 18 19 20 21 22 23; do
	variable="CLANG${version}_CXX"
	[[ -z ${!variable:-} ]] || add_compiler "${!variable}"
	add_compiler "clang++-$version"
done
[[ -z ${CLANG_CXX:-} ]] || add_compiler "$CLANG_CXX"
add_compiler clang++

mapfile -t cases < <(find "$test_dir" -maxdepth 1 -name '*fail.cc' -print | sort)
if (( ${#cases[@]} != 24 )); then
	printf 'format rejection matrix: expected 24 compile-fail cases, found %d\n' "${#cases[@]}" >&2
	exit 1
fi
positive_cases=(
	"$test_dir/printf_argument_validation.cc"
	"$test_dir/format_consteval.cc"
	"$test_dir/static_format_endpoint.cc"
)

compiler_count=0
for compiler in "${compilers[@]}"; do
	version_line=$($compiler --version | head -n 1)
	if [[ $version_line == *clang* ]]; then
		major=$(printf '%s\n' "$version_line" | sed -nE 's/.*clang version ([0-9]+).*/\1/p')
		[[ -n $major && $major -ge 17 && $major -le 23 ]] || continue
	else
		major=$($compiler -dumpversion | sed -E 's/^([0-9]+).*/\1/')
		[[ -n $major && $major -ge 13 && $major -le 16 ]] || continue
	fi
	compiler_count=$((compiler_count + 1))
	for source in "${cases[@]}"; do
		log="$work_dir/${compiler_count}_$(basename "$source").log"
		if "$compiler" "${common_flags[@]}" "$source" >"$log" 2>&1; then
			printf 'format rejection matrix: %s unexpectedly accepted %s\n' \
				"$version_line" "$(basename "$source")" >&2
			exit 1
		fi
	done
	for source in "${positive_cases[@]}"; do
		log="$work_dir/${compiler_count}_positive_$(basename "$source").log"
		if ! "$compiler" "${common_flags[@]}" "$source" >"$log" 2>&1; then
			printf 'format rejection matrix: %s rejected positive control %s\n' \
				"$version_line" "$(basename "$source")" >&2
			cat "$log" >&2
			exit 1
		fi
	done
	printf 'format rejection matrix: PASS 24/24 rejects + 3/3 controls (%s)\n' \
		"$version_line"
done

if (( compiler_count == 0 )); then
	printf 'format rejection matrix: no GCC 13-16 or Clang 17-23 compiler found\n' >&2
	exit 1
fi
