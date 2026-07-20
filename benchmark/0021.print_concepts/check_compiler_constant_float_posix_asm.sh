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

awk '
	/^0*[0-9a-f]+ <fast_io_fmt_constant_float_only>:/ { in_probe = 1; next }
	in_probe && /^$/ { exit }
	in_probe { print }
' "$temporary/probe.dump" >"$temporary/field-only.dump"

test "$(grep -c -E '[[:space:]]syscall([[:space:]]|$)' "$temporary/main.dump")" -eq 1
# GCC commonly splits `i=3.2` into a four-byte and a one-byte store, whereas
# Clang materializes the complete five-byte record in one wider immediate.
# Accept both encodings while still proving every payload byte is present in
# the caller and no formatter survives.
if ! grep -q -E 'mov[[:space:]].*0x2e333d69' "$temporary/main.dump"; then
	grep -q -E 'mov(abs)?[[:space:]].*0x322e333d69' "$temporary/main.dump"
else
	grep -q -E 'mov[[:space:]].*0x32' "$temporary/main.dump"
fi
! grep -q -E '[[:space:]]call[[:space:]]|writev|scatter|cold_impl' "$temporary/main.dump"
! grep -q -E 'sub[[:space:]]+rsp' "$temporary/main.dump"

# This separate function proves that the literal double survives the complete
# fmt entry/lower/semantic transport into core's compiler-constant gate.  A
# formatter call here means that some fmt adapter turned the known source into
# an opaque run-time object even if the prefixed whole-record probe still folds.
test "$(grep -c -E '[[:space:]]syscall([[:space:]]|$)' "$temporary/field-only.dump")" -eq 1
if ! grep -q -E 'mov[[:space:]].*0x34312e33' "$temporary/field-only.dump"; then
	# Clang commonly splits the same four bytes into byte/word stores.
	grep -q -E 'mov[[:space:]].*0x2e00' "$temporary/field-only.dump"
	grep -q -E 'mov[[:space:]].*0x33' "$temporary/field-only.dump"
	grep -q -E 'mov[[:space:]].*0x3431' "$temporary/field-only.dump"
fi
! grep -q -E '[[:space:]]call[[:space:]]|writev|scatter|cold_impl' "$temporary/field-only.dump"

# GCC may partition the syscall-error trap.  That block is allowed to contain
# only the terminating trap and alignment; a successful write, helper call, or
# normal return in it is a regression.
if test -s "$temporary/main-cold.dump"; then
	grep -q -E '[[:space:]]ud2([[:space:]]|$)' "$temporary/main-cold.dump"
	! grep -q -E '[[:space:]]call[[:space:]]|[[:space:]]syscall([[:space:]]|$)|[[:space:]]ret([[:space:]]|$)|writev|scatter|cold_impl' \
		"$temporary/main-cold.dump"
fi
