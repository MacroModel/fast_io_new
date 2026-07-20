#!/usr/bin/env bash
set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=${ROOT:-$(CDPATH= cd -- "$script_dir/../.." && pwd)}
source_file=${SOURCE:-"$script_dir/static_argument_fixed_width_asm.cc"}
objdump_bin=${OBJDUMP:-objdump}
temporary=$(mktemp -d)
trap 'find "$temporary" -type f -delete; rmdir "$temporary"' EXIT

pin=()
if [[ -n ${TASKSET_CPU:-} ]] && command -v taskset >/dev/null 2>&1; then
	pin=(taskset -c "$TASKSET_CPU")
fi

extract_symbol()
{
	local dump=$1
	local symbol=$2
	local output=$3
	awk -v symbol="$symbol" '
		$0 ~ "<" symbol ">:" { in_symbol = 1; next }
		in_symbol && /^$/ { exit }
		in_symbol { print }
	' "$dump" >"$output"
}

check_compiler()
{
	local cxx=$1
	local tag=$2
	local executable="$temporary/$tag"
	local dump="$temporary/$tag.dump"
	local symbols="$temporary/$tag.symbols"
	local actual="$temporary/$tag.out"
	local expected="$temporary/$tag.expected"

	"${pin[@]}" "$cxx" -I"$root/include" -std=c++20 -O3 \
		-fno-exceptions -fno-rtti -fno-stack-protector \
		-fcf-protection=none -fno-pie -no-pie "$source_file" \
		-o "$executable"
	"${pin[@]}" "$executable" >"$actual"
	printf '%s' \
		'i = 0000000000012.440000i = +000000000012.440000i = *****12.440000******' \
		>"$expected"
	cmp "$expected" "$actual"

	"$objdump_bin" -drwC -Mintel "$executable" >"$dump"
	"$objdump_bin" -tC "$executable" >"$symbols"

	local static_symbol
	for static_symbol in brace_static_fixed printf_static_fixed brace_static_middle; do
		local body="$temporary/$tag.$static_symbol"
		extract_symbol "$dump" "$static_symbol" "$body"
		test -s "$body"
		test "$(grep -c -E '[[:space:]]syscall([[:space:]]|$)' "$body")" -eq 1
		! grep -q -E '[[:space:]]call[[:space:]]|sub[[:space:]]+rsp|writev|scatter|floating_|compiler_constant' "$body"
	done

	# Each complete 24-byte spelling must be owned by the compiled format
	# program in read-only storage, rather than reconstructed on the stack.
	test "$(grep -c -E '[.]rodata.*compiled_static_format_program.*::storage$' "$symbols")" -eq 3
	grep -q -F 'i = 0000000000012.440000' "$executable"
	grep -q -F 'i = +000000000012.440000' "$executable"
	grep -q -F 'i = *****12.440000******' "$executable"

	local runtime_body="$temporary/$tag.runtime"
	extract_symbol "$dump" brace_runtime_fixed "$runtime_body"
	test -s "$runtime_body"
	! grep -q -E 'compiled_static_format_program|compiler_constant' "$runtime_body"
}

tested=0
for cxx in "${GCC13_CXX:-g++-13}" "${GCC14_CXX:-g++-14}" \
	"${GCC15_CXX:-g++-15}" "${GCC16_CXX:-g++-16}"; do
	if command -v "$cxx" >/dev/null 2>&1; then
		check_compiler "$cxx" "${cxx##*/}"
		tested=$((tested + 1))
	fi
done

for version in 17 18 19 20 21 22 23; do
	variable="CLANG${version}_CXX"
	cxx=${!variable:-clang++-${version}}
	if command -v "$cxx" >/dev/null 2>&1; then
		check_compiler "$cxx" "${cxx##*/}"
		tested=$((tested + 1))
	fi
done

if [[ -n ${CLANG_CXX:-} ]] && command -v "$CLANG_CXX" >/dev/null 2>&1; then
	check_compiler "$CLANG_CXX" clang-current
	tested=$((tested + 1))
fi

if (( tested == 0 )); then
	echo "no supported GCC or Clang executable found" >&2
	exit 77
fi

