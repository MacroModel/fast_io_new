#!/usr/bin/env bash
set -euo pipefail

root=${ROOT:-$(cd "$(dirname "$0")/../.." && pwd)}
source_file="$root/benchmark/0021.print_concepts/compiler_constant_integer_facade_asm.cc"
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT

declare -a compilers=()
declare -A seen=()

add_compiler()
{
	local candidate=$1
	if ! command -v "$candidate" >/dev/null 2>&1; then
		return 0
	fi
	local path
	path=$(command -v "$candidate")
	local identity
	identity=$(readlink -f "$path" 2>/dev/null || printf '%s' "$path")
	if [[ -z ${seen[$identity]+x} ]]; then
		seen[$identity]=1
		compilers+=("$path")
	fi
}

for version in 13 14 15 16; do
	variable="GCC${version}_CXX"
	if [[ -n ${!variable:-} ]]; then
		add_compiler "${!variable}"
	fi
	add_compiler "g++-$version"
done
add_compiler g++

for version in 17 18 19 20 21 22 23; do
	variable="CLANG${version}_CXX"
	if [[ -n ${!variable:-} ]]; then
		add_compiler "${!variable}"
	fi
	add_compiler "clang++-$version"
done
if [[ -n ${CLANG_CXX:-} ]]; then
	add_compiler "$CLANG_CXX"
fi
add_compiler clang++

fail()
{
	printf 'compiler-constant integer facade asm gate: %s\n' "$*" >&2
	exit 1
}

extract_function()
{
	local assembly=$1
	local symbol=$2
	local destination=$3
	awk -v symbol="$symbol" '
		$0 == symbol ":" || $0 ~ "^" symbol ":[[:space:]]" { active = 1 }
		active { print }
		active && $0 ~ "^[[:space:]]*\\.size[[:space:]]+" symbol "([,[:space:]]|$)" { exit }
	' "$assembly" > "$destination"
	[[ -s $destination ]] || fail "cannot locate $symbol in $assembly"
}

compiler_count=0
for compiler in "${compilers[@]}"; do
	version_line=$($compiler --version | head -n 1)
	if [[ $version_line == *clang* ]]; then
		major=$(printf '%s\n' "$version_line" | sed -nE 's/.*clang version ([0-9]+).*/\1/p')
		[[ -n $major && $major -ge 17 && $major -le 23 ]] || continue
		family=clang
	else
		major=$($compiler -dumpversion | sed -E 's/^([0-9]+).*/\1/')
		[[ -n $major && $major -ge 13 && $major -le 16 ]] || continue
		family=gcc
	fi

	compiler_count=$((compiler_count + 1))
	name="${family}${major}_${compiler_count}"
	assembly="$work_dir/$name.s"
	object="$work_dir/$name.o"
	compile_log="$work_dir/$name.log"
	common_flags=(
		-std=c++20 -O3 -fno-exceptions -fno-rtti
		-fno-stack-protector -fcf-protection=none
		-fno-asynchronous-unwind-tables -fno-unwind-tables
		-ffunction-sections -fdata-sections -I"$root/include"
	)
	if ! "$compiler" "${common_flags[@]}" -S "$source_file" -o "$assembly" >"$compile_log" 2>&1; then
		fail "$version_line failed to compile the assembly probe (see $compile_log)"
	fi
	if ! "$compiler" "${common_flags[@]}" -c "$source_file" -o "$object" >>"$compile_log" 2>&1; then
		fail "$version_line failed to compile the object probe (see $compile_log)"
	fi

	default_body="$work_dir/$name.default"
	constant_body="$work_dir/$name.constant"
	runtime_body="$work_dir/$name.runtime"
	extract_function "$assembly" fast_io_compiler_constant_default_fmt_int32 "$default_body"
	extract_function "$assembly" fast_io_compiler_constant_posix_fmt_int32 "$constant_body"
	extract_function "$assembly" fast_io_compiler_constant_posix_fmt_runtime_int "$runtime_body"

	if rg -q 'jeaiii|print_reserve_integral_define' "$default_body"; then
		fail "$version_line routes default fmt literal 32 through a runtime integer writer"
	fi
	if rg -q 'jeaiii|print_reserve_integral_(compiler_constant_)?define' "$constant_body"; then
		fail "$version_line routes explicit fmt literal 32 through an integer writer"
	fi
	if ! rg -q 'flockfile' "$default_body"; then
		fail "$version_line no longer locks the default C stdout facade"
	fi
	payload_pattern='\$(540876905|12851)|\$0x(203[dD]2069|3233)'
	lock_line=$(rg -n -m1 'flockfile' "$default_body" | cut -d: -f1)
	payload_line=$(rg -n -m1 "$payload_pattern" "$default_body" | cut -d: -f1 || true)
	if [[ -z $payload_line ]]; then
		fail "$version_line did not fold default fmt literal 32 to its constant payload"
	fi
	if (( payload_line <= lock_line )); then
		fail "$version_line materializes default fmt literal 32 before acquiring stdout's lock"
	fi

	# The explicit unbuffered record may keep automatic iovec descriptors, but every character range must point at an
	# immutable provider.  Reject the previous compact-buffer immediates and require both provider symbols in code and
	# in actual rodata sections of the object.
	if rg -q "$payload_pattern|memcpy" "$constant_body"; then
		fail "$version_line copies the explicit fmt literal 32 character payload through automatic storage"
	fi
	if ! rg -q 'compiled_literal_run.*storage' "$constant_body"; then
		fail "$version_line lost the compiled format-literal provider in the explicit constant record"
	fi
	if ! rg -q 'compiler_constant_integral_pair_fragments' "$constant_body"; then
		fail "$version_line lost the two-digit static integer provider in the explicit constant record"
	fi
	section_table="$work_dir/$name.sections"
	readelf -SW "$object" > "$section_table"
	if ! rg -q '\.rodata\..*compiled_literal_run.*storage' "$section_table"; then
		fail "$version_line did not place the compiled literal provider in rodata"
	fi
	if ! rg -q '\.rodata\..*compiler_constant_integral_pair_fragments' "$section_table"; then
		fail "$version_line did not place the integer pair provider in rodata"
	fi

	# An optimizer-unknown value must retain the historical dynamic writer and must not acquire a reference to the
	# compiler-constant digit provider.  This is the zero-residue half of the value gate.
	if rg -q 'compiler_constant_integral_(pair|digit)_fragments|compiler_constant_scalar_manip' "$runtime_body"; then
		fail "$version_line leaks compiler-constant integer state into the runtime-value facade"
	fi

	printf 'compiler-constant integer facade asm gate: PASS (%s)\n' "$version_line"
done

(( compiler_count != 0 )) || fail 'no GCC 13-16 or Clang 17-23 compiler was found'
