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
	identity=$("$path" --version 2>/dev/null | head -n 1 || true)
	[[ -n $identity ]] || identity=$(readlink -f "$path" 2>/dev/null || printf '%s' "$path")
	if [[ -z ${seen[$identity]+x} ]]; then
		seen[$identity]=1
		compilers+=("$path")
	fi
}

for version in 11 12 13 14 15 16 17; do
	variable="GCC${version}_CXX"
	[[ -z ${!variable:-} ]] || add_compiler "${!variable}"
	add_compiler "g++-$version"
done
add_compiler g++
for version in 13 14 15 16 17 18 19 20 21 22 23; do
	variable="CLANG${version}_CXX"
	[[ -z ${!variable:-} ]] || add_compiler "${!variable}"
	add_compiler "clang++-$version"
done
[[ -z ${CLANG_CXX:-} ]] || add_compiler "$CLANG_CXX"
add_compiler clang++

mapfile -t cases < <(find "$test_dir" -maxdepth 1 -name '*fail.cc' -print | sort)
if (( ${#cases[@]} != 51 )); then
	printf 'format rejection matrix: expected 51 compile-fail cases, found %d\n' "${#cases[@]}" >&2
	exit 1
fi
positive_cases=(
	"$test_dir/printf_argument_validation.cc"
	"$test_dir/format_argument_validation_dispatch_contract.cc"
	"$test_dir/format_consteval.cc"
	"$test_dir/format_semantic_lowering.cc"
	"$test_dir/static_custom_format_arg.cc"
	"$test_dir/static_format_endpoint.cc"
	"$test_dir/static_format_output_provider_contract.cc"
)

compiler_count=0
for compiler in "${compilers[@]}"; do
	version_line=$($compiler --version | head -n 1)
	if [[ $version_line == *clang* ]]; then
		major=$(printf '%s\n' "$version_line" | sed -nE 's/.*clang version ([0-9]+).*/\1/p')
		[[ -n $major && $major -ge 13 && $major -le 23 ]] || continue
		family=clang
	else
		major=$($compiler -dumpversion | sed -E 's/^([0-9]+).*/\1/')
		[[ -n $major && $major -ge 11 && $major -le 17 ]] || continue
		family=gcc
	fi
	compiler_count=$((compiler_count + 1))
	compiler_flags=("${common_flags[@]}")
	# The downloaded GCC 13/14 binaries use the host libstdc++ threading
	# headers; this selects their compatible initialization declaration only.
	if [[ $family == gcc && ($major -eq 13 || $major -eq 14) ]]; then
		compiler_flags+=(-D_GTHREAD_USE_COND_INIT_FUNC)
	fi
	if [[ $family == clang ]]; then
		clang_gcc_toolchain=
		case $major in
			13|14|15)
				clang_gcc_toolchain=${FAST_IO_GCC12_TOOLCHAIN_ROOT:-/tmp/fast_io_toolchains/gcc/toolchains/gcc-12.5.0}
				;;
			16|17)
				clang_gcc_toolchain=${FAST_IO_GCC13_TOOLCHAIN_ROOT:-/tmp/fast_io_toolchains/gcc/toolchains/gcc-13.4.0}
				;;
			18|19|20)
				clang_gcc_toolchain=${FAST_IO_GCC14_TOOLCHAIN_ROOT:-/tmp/fast_io_toolchains/gcc/toolchains/gcc-14.4.0}
				;;
		esac
		if [[ -n $clang_gcc_toolchain && -d $clang_gcc_toolchain ]]; then
			compiler_flags+=(--gcc-toolchain="$clang_gcc_toolchain")
		fi
	fi
	for source in "${cases[@]}"; do
		log="$work_dir/${compiler_count}_$(basename "$source").log"
		if "$compiler" "${compiler_flags[@]}" "$source" >"$log" 2>&1; then
			printf 'format rejection matrix: %s unexpectedly accepted %s\n' \
				"$version_line" "$(basename "$source")" >&2
			exit 1
		fi
		case $(basename "$source") in
			format_argument_brace_adl_poison_fail.cc|\
			format_argument_brace_large_dispatch_hole_fail.cc|\
			format_argument_brace_large_static_hole_fail.cc|\
			format_argument_printf_large_dispatch_hole_fail.cc)
				# These regressions must reach the mandatory domain assertion. A
				# failure in unrelated formatting machinery proves no safety property.
				if ! grep -Fq \
					'fast_io format: the grammar rejected the supplied argument domain' \
					"$log"; then
					printf 'format rejection matrix: %s rejected %s for the wrong reason\n' \
						"$version_line" "$(basename "$source")" >&2
					cat "$log" >&2
					exit 1
				fi
				;;
		esac
	done
	compiler_positive_cases=("${positive_cases[@]}")
	if [[ $family == gcc && $major -eq 11 ]]; then
		# libstdc++ 11 does not provide a literal C++20 std::string, and GCC 11
		# also has a known unrelated tuple-fallback classification difference in
		# the broad custom-format integration test. Keep all 51 rejection cases,
		# then use the focused provider contract plus the four portable positive
		# controls to prove that success remains possible on this frontend.
		compiler_positive_cases=(
			"$test_dir/printf_argument_validation.cc"
			"$test_dir/format_argument_validation_dispatch_contract.cc"
			"$test_dir/format_semantic_lowering.cc"
			"$test_dir/static_format_endpoint.cc"
			"$test_dir/static_format_output_provider_contract.cc"
		)
	fi
	for source in "${compiler_positive_cases[@]}"; do
		log="$work_dir/${compiler_count}_positive_$(basename "$source").log"
		if ! "$compiler" "${compiler_flags[@]}" "$source" >"$log" 2>&1; then
			printf 'format rejection matrix: %s rejected positive control %s\n' \
				"$version_line" "$(basename "$source")" >&2
			cat "$log" >&2
			exit 1
		fi
	done
	printf 'format rejection matrix: PASS 51/51 rejects + %d/%d controls (%s)\n' \
		"${#compiler_positive_cases[@]}" "${#compiler_positive_cases[@]}" \
		"$version_line"
done

if (( compiler_count == 0 )); then
	printf 'format rejection matrix: no GCC 11-17 or Clang 13-23 compiler found\n' >&2
	exit 1
fi
