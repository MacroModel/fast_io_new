#!/usr/bin/env bash
set -euo pipefail

root=${ROOT:-$(cd "$(dirname "$0")/../.." && pwd)}
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT

fail()
{
	printf 'compiler-constant public facade gate: %s\n' "$*" >&2
	exit 1
}

require_count()
{
	local expected=$1
	local pattern=$2
	local file=$3
	local actual
	actual=$(rg -c "$pattern" "$file" || true)
	[[ $actual == "$expected" ]] || fail "$file: expected $expected matches for $pattern, found ${actual:-0}"
}

declaration_prefix()
{
	local file=$1
	local signature=$2
	local destination=$3
	local line
	line=$(rg -n -m1 -F "$signature" "$file" | cut -d: -f1)
	[[ -n $line ]] || fail "$file: cannot find $signature"
	local first=$((line > 12 ? line - 12 : 1))
	sed -n "${first},${line}p" "$file" > "$destination"
}

function_body()
{
	local file=$1
	local signature=$2
	local destination=$3
	awk -v signature="$signature" '
		!active && index($0, signature) { active = 1 }
		active {
			print
			line = $0
			opens = gsub(/\{/, "{", line)
			line = $0
			closes = gsub(/\}/, "}", line)
			depth += opens - closes
			if (opens != 0) seen = 1
			if (seen && depth == 0) exit
		}
	' "$file" > "$destination"
	[[ -s $destination ]] || fail "$file: cannot extract $signature"
}

io_header="$root/include/fast_io_legacy_impl/io.h"
defined_header="$root/include/fast_io_legacy_impl/defined_types.h"
core_print_header="$root/include/fast_io_core_impl/operations/printimpl/print_freestanding_cxx20.h"
format_print_header="$root/include/fast_io_format/print.h"

for name in print println debug_print debug_println; do
	prefix="$work_dir/$name.prefix"
	body="$work_dir/$name.body"
	declaration_prefix "$io_header" "inline constexpr void $name(" "$prefix"
	function_body "$io_header" "inline constexpr void $name(" "$body"
	rg -q 'always_inline|forceinline' "$prefix" || fail "$name is not forced inline"
	rg -q 'print_freestanding_compiler_constant_pre_normalization<' "$body" || \
		fail "$name bypasses the shared source compiler-constant gate"
done

for name in perr perrln; do
	prefix="$work_dir/$name.prefix"
	body="$work_dir/$name.body"
	declaration_prefix "$io_header" "inline constexpr void $name(" "$prefix"
	function_body "$io_header" "inline constexpr void $name(" "$body"
	! rg -q 'always_inline|forceinline' "$prefix" || fail "$name must remain ordinary inline"
	rg -q 'print_freestanding_compiler_constant_pre_normalization_cold<' "$body" || \
		fail "$name bypasses the shared cold source compiler-constant gate"
done

for name in panic panicln; do
	prefix="$work_dir/$name.prefix"
	body="$work_dir/$name.body"
	declaration_prefix "$io_header" "inline constexpr void $name(" "$prefix"
	function_body "$io_header" "inline constexpr void $name(" "$body"
	! rg -q 'always_inline|forceinline' "$prefix" || fail "$name must remain ordinary inline"
	rg -q 'panic_try_compiler_constant_pre_normalization' "$body" || \
		fail "$name bypasses panic's source compiler-constant gate"
done

for name in debug_perr debug_perrln; do
	prefix="$work_dir/$name.prefix"
	body="$work_dir/$name.body"
	declaration_prefix "$io_header" "inline constexpr void $name(" "$prefix"
	function_body "$io_header" "inline constexpr void $name(" "$body"
	! rg -q 'always_inline|forceinline' "$prefix" || fail "$name must remain ordinary inline"
	if [[ $name == debug_perr ]]; then
		rg -q '::fast_io::io::perr\(' "$body" || fail "$name no longer shares perr's gate"
	else
		rg -q '::fast_io::io::perrln\(' "$body" || fail "$name no longer shares perrln's gate"
	fi
done

require_count 4 'print_freestanding_compiler_constant_pre_normalization<' "$io_header"
require_count 2 'print_freestanding_compiler_constant_pre_normalization_cold<' "$io_header"
require_count 2 'panic_try_compiler_constant_pre_normalization' "$io_header"
require_count 1 'print_freestanding_compiler_constant_pre_normalization<line>' "$defined_header"
require_count 1 'print_freestanding_compiler_constant_pre_normalization_cold<line>' "$defined_header"
require_count 1 'print_freestanding_compiler_constant_pre_normalization<line>' "$core_print_header"
rg -q 'print_freestanding_compiler_constant_pre_normalization<false>' "$format_print_header" || \
	fail 'fmt ordinary lowered print bypasses the core source gate'
rg -q 'print_freestanding_compiler_constant_pre_normalization_passive_mixed<false>' "$format_print_header" || \
	fail 'fmt passive lowered print bypasses the core source gate'
rg -q 'print_passive_mixed_put_area_fast_entry<line>' "$core_print_header" || \
	fail 'fmt passive gate lost its exact dynamic false continuation'

require_count 10 'basic_general_concat_compiler_constant_checked_entry' \
	"$root/include/fast_io_unit/string_impl/concat_std.h"
require_count 10 'basic_general_concat_compiler_constant_checked_entry' \
	"$root/include/fast_io_unit/string_impl/concat.h"
require_count 22 'basic_general_concat_compiler_constant_checked_entry' \
	"$root/include/fast_io_dsal/string.h"
for file in "$root/include/fast_io_format/concat_std.h" \
	"$root/include/fast_io_format/concat_fast_io.h"; do
	require_count 10 'concat_builtin_with_rule' "$file"
	require_count 10 '= delete;' "$file"
done
require_count 2 'basic_general_concat_compiler_constant_checked_entry' \
	"$root/include/fast_io_format/details/concat.h"

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

valid_tests=(
	"$root/tests/0002.printscan/compiler_constant_public_facades.cc"
	"$root/tests/0003.concat/format_consteval.cc"
	"$root/tests/0003.concat/printf_argument_validation.cc"
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
	for source in "${valid_tests[@]}"; do
		binary="$work_dir/${compiler_count}_$(basename "$source" .cc)"
		log="$binary.log"
		if ! "$compiler" -std=c++20 -O2 -fno-exceptions -fno-rtti \
			-I"$root/include" "$source" -o "$binary" >"$log" 2>&1; then
			fail "$version_line failed to compile $(basename "$source") (see $log)"
		fi
		"$binary" || fail "$version_line failed $(basename "$source") at runtime"
	done
	printf 'compiler-constant public facade gate: PASS source/runtime (%s)\n' "$version_line"
done
(( compiler_count != 0 )) || fail 'no GCC 13-16 or Clang 17-23 compiler was found'

ROOT="$root" bash "$root/benchmark/0021.print_concepts/check_compiler_constant_integer_facade_asm.sh"
ROOT="$root" bash "$root/tests/0003.concat/check_format_rejection_matrix.sh"
