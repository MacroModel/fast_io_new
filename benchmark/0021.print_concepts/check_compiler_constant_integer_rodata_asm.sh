#!/usr/bin/env bash
set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=${ROOT:-$(CDPATH= cd -- "$script_dir/../.." && pwd)}
cxx=${CXX:-c++}
objdump_bin=${OBJDUMP:-objdump}
objcopy_bin=${OBJCOPY:-objcopy}
nm_bin=${NM:-nm}
temporary=$(mktemp -d)
trap 'find "$temporary" -type f -delete; rmdir "$temporary"' EXIT

common_flags=(
	-I"$root/include"
	-std=c++20
	-O3
	-fno-exceptions
	-fno-rtti
	-fno-stack-protector
	-fcf-protection=none
)
source_file="$script_dir/compiler_constant_integer_rodata_asm.cc"

compile_probe()
{
	local mode=$1
	local define=()
	if test "$mode" = explicit
	then
		define=(-DFAST_IO_COMPILER_CONSTANT_INTEGER_EXPLICIT_OUT)
	elif test "$mode" = static
	then
		define=(-DFAST_IO_COMPILER_CONSTANT_INTEGER_STATIC_OUT)
	fi
	"$cxx" "${common_flags[@]}" "${define[@]}" "$source_file" \
		-o "$temporary/$mode"
	"$objdump_bin" -drwC -Mintel "$temporary/$mode" \
		>"$temporary/$mode.dump"
	awk '
		/^[[:xdigit:]]+ <main>:/ { in_main = 1; next }
		in_main && /^$/ { exit }
		in_main { print }
	' "$temporary/$mode.dump" >"$temporary/$mode.main"
	"$temporary/$mode" >"$temporary/$mode.stdout"
	cmp -s "$temporary/$mode.stdout" <(printf 'i = 32')
}

compile_probe default
compile_probe explicit
compile_probe static

# A literal argument must not leave either the mature integer formatter or the
# compiler-constant integer writer in the reachable executable.  Checking the
# symbol table, rather than only main, also covers the default FILE-buffer
# capacity fallback.
"$nm_bin" -C "$temporary/default" >"$temporary/default.nm"
! grep -q -E 'jeaiii|print_reserve_integral_(compiler_constant_)?define' \
	"$temporary/default.nm"

# A plain function argument cannot become a value-dependent static object: the
# same template specialization also accepts a different caller value.  The
# optimizer-proven spelling is therefore one contiguous automatic DSAL record,
# but it must still use one scalar syscall and must not build scatter metadata.
"$nm_bin" -C "$temporary/explicit" >"$temporary/explicit.nm"
! grep -q -E 'jeaiii|print_reserve_integral_(compiler_constant_)?define' \
	"$temporary/explicit.nm"
! grep -q -E 'scatter_write|print_static_scatter' \
	"$temporary/explicit.nm"
! grep -q -E '[[:space:]]call[[:space:]]' "$temporary/explicit.main"
test "$(grep -c -E '[[:space:]]syscall([[:space:]]|$)' \
	"$temporary/explicit.main")" -eq 1

# static_arg places the value in the type graph, so the format program can own
# one merged DSAL array in rodata and pass that provider directly to write-all.
"$nm_bin" -C "$temporary/static" >"$temporary/static.nm"
! grep -q -E 'jeaiii|print_reserve_integral_(compiler_constant_)?define|scatter_write' \
	"$temporary/static.nm"
! grep -q -E '(^|[^[:alnum:]_])(rsp|esp)([^[:alnum:]_]|$)|[[:space:]]push[[:space:]]|[[:space:]]pop[[:space:]]' \
	"$temporary/static.main"
! grep -q -E '[[:space:]]call[[:space:]]' "$temporary/static.main"
test "$(grep -c -E '[[:space:]]syscall([[:space:]]|$)' \
	"$temporary/static.main")" -eq 1
"$objcopy_bin" --dump-section .rodata="$temporary/static.rodata" \
	"$temporary/static"
grep -aFq 'i = 32' "$temporary/static.rodata"

printf 'PASS %s\n' "$("$cxx" --version | head -n 1)"
