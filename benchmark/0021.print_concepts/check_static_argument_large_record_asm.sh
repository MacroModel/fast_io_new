#!/usr/bin/env bash
set -euo pipefail

root=${ROOT:-$(cd "$(dirname "$0")/../.." && pwd)}
source_file=${SOURCE:-"$root/benchmark/0021.print_concepts/static_argument_large_record_asm.cc"}
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT
max_rss_kib=${FAST_IO_STATIC_ARGUMENT_MAX_RSS_KIB:-786432}

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

for version in 11 12 13 14 15 16; do
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

fail()
{
	printf 'static-argument large-record asm gate: %s\n' "$*" >&2
	exit 1
}

extract_function()
{
	local assembly=$1 symbol=$2 destination=$3
	awk -v symbol="$symbol" '
		$0 == symbol ":" || $0 ~ "^" symbol ":[[:space:]]" { active = 1 }
		active { print }
		active && $0 ~ "^[[:space:]]*\\.size[[:space:]]+" symbol "([,[:space:]]|$)" { exit }
	' "$assembly" >"$destination"
	[[ -s $destination ]] || fail "cannot locate $symbol in $assembly"
}

check_body()
{
	local version_line=$1 body=$2 symbol=$3
	if rg -q 'jeaiii|floating_|print_reserve_|writev|scatter_write|print_static_scatter' "$body"; then
		fail "$version_line routes $symbol through formatting or scatter output"
	fi
	if rg -q '(^|[^[:alnum:]_])(rsp|esp)([^[:alnum:]_]|$)|[[:space:]]push[[:space:]]|[[:space:]]pop[[:space:]]|[[:space:]]call[q]?[[:space:]]' "$body"; then
		fail "$version_line copies or calls while emitting $symbol"
	fi
	if [[ $(rg -c 'syscall' "$body") -ne 1 ]]; then
		fail "$version_line does not give $symbol exactly one scalar syscall loop"
	fi
	if ! rg -q 'static_provider_storage_t.*compiled_static_replacement_provider.*storage' "$body"; then
		fail "$version_line does not address $symbol's core replacement provider"
	fi
}

check_byte_run()
{
	local rodata=$1 byte=$2 required=$3
	od -An -v -tu1 "$rodata" | awk -v byte="$byte" -v required="$required" '
		{
			for (field = 1; field <= NF; ++field) {
				if ($field == byte) {
					++run;
					if (maximum < run) maximum = run;
				} else {
					run = 0;
				}
			}
		}
		END { exit maximum < required }
	' || fail "linked rodata has no contiguous $required-byte run of value $byte"
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
		[[ -n $major && $major -ge 11 && $major -le 16 ]] || continue
		family=gcc
	fi

	compiler_count=$((compiler_count + 1))
	name="${family}${major}_${compiler_count}"
	assembly="$work_dir/$name.s"
	executable="$work_dir/$name"
	compile_log="$work_dir/$name.log"
	rss_file="$work_dir/$name.rss"
	common_flags=(
		-std=c++20 -O3 -fno-exceptions -fno-rtti
		-fno-stack-protector -fcf-protection=none
		-fno-asynchronous-unwind-tables -fno-unwind-tables
		-ffunction-sections -fdata-sections -I"$root/include"
	)
	if command -v /usr/bin/time >/dev/null 2>&1; then
		if ! /usr/bin/time -f '%M' -o "$rss_file" \
			"$compiler" "${common_flags[@]}" -S "$source_file" -o "$assembly" \
			>"$compile_log" 2>&1; then
			fail "$version_line failed to compile the maximum record (see $compile_log)"
		fi
		rss_kib=$(<"$rss_file")
		if (( rss_kib > max_rss_kib )); then
			fail "$version_line used ${rss_kib} KiB RSS (limit ${max_rss_kib} KiB)"
		fi
	else
		if ! "$compiler" "${common_flags[@]}" -S "$source_file" -o "$assembly" \
			>"$compile_log" 2>&1; then
			fail "$version_line failed to compile the maximum record (see $compile_log)"
		fi
		rss_kib=unmeasured
	fi

	for symbol in \
		fast_io_static_argument_record_65 \
		fast_io_static_argument_record_4k \
		fast_io_static_argument_record_limit; do
		body="$work_dir/$name.$symbol"
		extract_function "$assembly" "$symbol" "$body"
		check_body "$version_line" "$body" "$symbol"
	done

	# Assemble the already-measured translation once, then inspect the linked
	# read-only section without repeating the expensive template front end.
	if ! "$compiler" -fno-exceptions -fno-rtti -fno-stack-protector \
		-fcf-protection=none "$assembly" -o "$executable" >>"$compile_log" 2>&1; then
		fail "$version_line failed to link the assembly probe (see $compile_log)"
	fi
	section_table="$work_dir/$name.sections"
	readelf -SW "$executable" >"$section_table"
	rg -q '[[:space:]]\.rodata[[:space:]]' "$section_table" || \
		fail "$version_line emitted no linked rodata section"
	rodata="$work_dir/$name.rodata"
	objcopy --dump-section .rodata="$rodata" "$executable"
	check_byte_run "$rodata" 97 65
	check_byte_run "$rodata" 98 4096
	check_byte_run "$rodata" 99 65536

	printf 'static-argument large-record asm gate: PASS (%s, max RSS %s KiB)\n' \
		"$version_line" "$rss_kib"
done

(( compiler_count != 0 )) || fail 'no GCC 11-16 or Clang 17-23 compiler was found'
