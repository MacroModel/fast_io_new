#!/usr/bin/env bash
set -euo pipefail

ROOT=${ROOT:-$(cd "$(dirname "$0")/../.." && pwd)}
SOURCE=${SOURCE:-"$ROOT/benchmark/0021.print_concepts/static_fragment_hot_policy_asm.cc"}
COMMON=(-std=c++20 -O3 -fno-exceptions -fno-rtti -fno-pie -fno-stack-protector -I"$ROOT/include" -S)

check_gcc() {
	local cxx=$1
	local output=$2
	"$cxx" "${COMMON[@]}" "$SOURCE" -o "$output"
	local body
	body=$(sed -n '/^fast_io_static_fragment_policy_probe:/,/^[[:space:]]*\.size[[:space:]]*fast_io_static_fragment_policy_probe/p' "$output")
	grep -q 'syscall' <<<"$body"
	if grep -q 'scatter_write_all.*cold_impl' <<<"$body"; then
		echo "$cxx: GCC hot-first probe re-entered the outlined cold completion" >&2
		return 1
	fi
}

check_clang() {
	local cxx=$1
	local output=$2
	"$cxx" "${COMMON[@]}" "$SOURCE" -o "$output"
	local body
	body=$(sed -n '/^fast_io_static_fragment_policy_probe:/,/^[[:space:]]*\.Lfunc_end/p' "$output")
	grep -q 'scatter_write_all.*cold_impl' <<<"$body"
	if grep -q 'syscall' <<<"$body"; then
		echo "$cxx: Clang probe unexpectedly inlined the GCC-only hot syscall" >&2
		return 1
	fi
}

tested=0
for cxx in "${GCC13_CXX:-g++-13}" "${GCC14_CXX:-g++-14}" \
	"${GCC15_CXX:-g++-15}" "${GCC16_CXX:-g++-16}"; do
	if command -v "$cxx" >/dev/null 2>&1; then
		check_gcc "$cxx" "/tmp/fast_io_static_fragment_policy_${cxx##*/}.s"
		tested=$((tested + 1))
	fi
done

for version in 17 18 19 20 21 22 23; do
	variable="CLANG${version}_CXX"
	cxx=${!variable:-clang++-${version}}
	if command -v "$cxx" >/dev/null 2>&1; then
		check_clang "$cxx" "/tmp/fast_io_static_fragment_policy_${cxx##*/}.s"
		tested=$((tested + 1))
	fi
done

# The development LLVM build is commonly installed as unversioned clang++.
if command -v clang++ >/dev/null 2>&1; then
	check_clang clang++ /tmp/fast_io_static_fragment_policy_clang-current.s
	tested=$((tested + 1))
fi

if (( tested == 0 )); then
	echo "no supported GCC or Clang executable found" >&2
	exit 77
fi

