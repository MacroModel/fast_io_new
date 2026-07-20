#!/usr/bin/env bash
set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=${ROOT:-$(CDPATH= cd -- "$script_dir/../.." && pwd)}
cxx=${CXX:-g++-15}
objdump_bin=${OBJDUMP:-objdump}
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT

"$cxx" -I"$root/include" -std=c++20 -O3 -fno-exceptions -fno-rtti \
	-fno-stack-protector -fcf-protection=none \
	"$script_dir/compiler_constant_float_posix_asm.cc" \
	-o "$temporary/probe"
"$objdump_bin" -drwC -Mintel "$temporary/probe" >"$temporary/probe.dump"

awk '
	/^0*[0-9a-f]+ <main>:/ { in_main = 1; next }
	in_main && /^$/ { exit }
	in_main { print }
' "$temporary/probe.dump" >"$temporary/main.dump"

awk '
	/^0*[0-9a-f]+ <main\.cold>:/ { in_cold = 1; next }
	in_cold && /^$/ { exit }
	in_cold { print }
' "$temporary/probe.dump" >"$temporary/main-cold.dump"

test "$(grep -c -E '[[:space:]]syscall([[:space:]]|$)' "$temporary/main.dump")" -eq 1
grep -q -E 'mov[[:space:]].*0x2e333d69' "$temporary/main.dump"
grep -q -E 'mov[[:space:]].*0x32' "$temporary/main.dump"
! grep -q -E '[[:space:]]call[[:space:]]|writev|scatter|cold_impl' "$temporary/main.dump"
! grep -q -E 'sub[[:space:]]+rsp' "$temporary/main.dump"

# GCC may partition the syscall-error trap.  That block is allowed to contain
# only the terminating trap and alignment; a successful write, helper call, or
# normal return in it is a regression.
if test -s "$temporary/main-cold.dump"; then
	grep -q -E '[[:space:]]ud2([[:space:]]|$)' "$temporary/main-cold.dump"
	! grep -q -E '[[:space:]]call[[:space:]]|[[:space:]]syscall([[:space:]]|$)|[[:space:]]ret([[:space:]]|$)|writev|scatter|cold_impl' \
		"$temporary/main-cold.dump"
fi
