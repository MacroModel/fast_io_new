#!/usr/bin/env bash
set -euo pipefail

root=${ROOT:-$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)}
source_file="$root/benchmark/0021.print_concepts/format_plain_float_lowering_asm.cc"
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT

if [[ $(uname -s) != Linux || $(uname -m) != x86_64 ]]; then
	printf 'plain floating lowering asm gate: SKIP (requires Linux x86_64 syscall assembly)\n'
	exit 0
fi

declare -a compilers=()
declare -A seen=()

add_compiler()
{
	local candidate=$1
	[[ -n $candidate ]] || return 0
	command -v "$candidate" >/dev/null 2>&1 || return 0
	local path identity version_line major
	path=$(command -v "$candidate")
	identity=$(readlink -f "$path" 2>/dev/null || printf '%s' "$path")
	[[ -z ${seen[$identity]+x} ]] || return 0
	version_line=$($path --version | head -n 1)
	[[ $version_line == *clang* ]] || return 0
	major=$(printf '%s\n' "$version_line" | sed -nE 's/.*clang version ([0-9]+).*/\1/p')
	[[ -n $major && $major -ge 23 ]] || return 0
	seen[$identity]=1
	compilers+=("$path")
}

add_compiler "${CLANG23_CXX:-}"
add_compiler clang++-23
add_compiler "${CLANG_TRUNK_CXX:-}"
add_compiler "${CXX:-}"
add_compiler clang++

fail()
{
	printf 'plain floating lowering asm gate: %s\n' "$*" >&2
	exit 1
}

extract_function()
{
	local assembly=$1 symbol=$2 destination=$3
	awk -v symbol="$symbol" '
		$0 == symbol ":" || $0 ~ "^" symbol ":[[:space:]]" { active = 1 }
		active { print }
		active && $0 ~ "^[[:space:]]*\\.size[[:space:]]+" symbol "([,[:space:]]|$)" { exit }
	' "$assembly" > "$destination"
	[[ -s $destination ]] || fail "cannot locate $symbol in $assembly"
}

check_constant_leaf()
{
	local identity=$1 body=$2 object=$3 symbol=$4
	if rg -q 'format_scalar_t|print_reserve_define|compiler_constant_floating' "$body"; then
		fail "$identity retained a format wrapper or floating conversion call"
	fi
	check_compact_constant_leaf "$identity" "$body" "$object" "$symbol"
	check_scalar_transport "$identity" "$body" 0
}

check_compact_constant_leaf()
{
	local identity=$1 body=$2 object=$3 symbol=$4
	local size_hex size_bytes instructions stack_allocation immediate value
	# Call classification alone is insufficient: a compiler could inline the
	# complete run-time ftoa graph and leave one syscall with no named converter.
	# Clang 23 emits 16--19 instructions and 59--75 bytes for these three legal
	# leaves.  The deliberately generous limits below admit more than five times
	# that instruction count and six times that symbol size, while rejecting an
	# accidentally inlined conversion engine independently of immediate spelling.
	size_hex=$(nm -S --defined-only "$object" | awk -v symbol="$symbol" \
		'$NF == symbol { print $2; exit }')
	[[ -n $size_hex ]] || fail "$identity has no measurable ELF symbol size"
	size_bytes=$((16#$size_hex))
	(( size_bytes <= 512 )) ||
		fail "$identity is not a compact scalar leaf (symbol size=$size_bytes bytes)"
	instructions=$(rg -c \
		'^[[:space:]]+[[:alpha:]_][[:alnum:]_.]*([[:space:]]|$)' \
		"$body" || true)
	instructions=${instructions:-0}
	(( instructions <= 96 )) ||
		fail "$identity is not a compact scalar leaf (instructions=$instructions)"
	stack_allocation=0
	while IFS= read -r immediate; do
		if [[ $immediate == 0x* ]]; then
			value=$((immediate))
		else
			value=$((10#$immediate))
		fi
		(( value <= stack_allocation )) || stack_allocation=$value
	done < <(sed -nE \
		's/^[[:space:]]*sub[qwl]?[[:space:]]+\$(0x[[:xdigit:]]+|[0-9]+),[[:space:]]*%rsp.*/\1/p' \
		"$body")
	if rg -q \
		'^[[:space:]]*(sub[qwl]?[[:space:]]+%[^,]+,|and[qwl]?[[:space:]]+[^,]+,)[[:space:]]*%rsp' \
		"$body"; then
		fail "$identity uses a dynamic or alignment-masked stack frame"
	fi
	(( stack_allocation <= 256 )) ||
		fail "$identity is not a compact scalar leaf (stack frame=$stack_allocation bytes)"
}

check_scalar_transport()
{
	local identity=$1 body=$2 conversion_calls=$3
	local syscalls external_calls external_tail_transfers external_transfers write_transfers
	# LLVM may keep the Linux syscall in the exported leaf or outline that exact
	# scalar transport behind one direct/tail call.  Count semantic transports,
	# rather than requiring one spelling, while still rejecting scatter, a cold
	# helper, an unclassified call, or two writes.
	if rg -q 'writev|scatter_write|write_all_bytes_cold' "$body"; then
		fail "$identity selected scatter or cold output for one compact scalar field"
	fi
	syscalls=$(rg -c '[[:space:]]syscall([[:space:]]|$)' "$body" || true)
	external_calls=$(rg -c '[[:space:]]callq?[[:space:]]' "$body" || true)
	external_tail_transfers=$(rg -c '[[:space:]]jmpq?[[:space:]]+(_Z|[[:alnum:]_]+@PLT)' "$body" || true)
	external_transfers=$((external_calls + external_tail_transfers))
	write_transfers=$(rg -c \
		'[[:space:]](callq?|jmpq?)[[:space:]].*(print_write_all_(direct|materialized)|write_some_bytes_overflow_define|posix_write_bytes_impl|system_call|write@PLT)' \
		"$body" || true)
	if (( syscalls == 1 && write_transfers == 0 && external_transfers == conversion_calls )); then
		return 0
	fi
	if (( syscalls == 0 && write_transfers == 1 && external_transfers == conversion_calls + 1 )); then
		return 0
	fi
	fail "$identity is not exactly one scalar transport (syscalls=$syscalls, write transfers=$write_transfers, other conversion calls=$conversion_calls, all external transfers=$external_transfers)"
}

check_runtime_leaf()
{
	local identity=$1 body=$2
	local conversion_calls
	if rg -q 'format_scalar_t|15format_scalar_t|compiler_constant_floating' "$body"; then
		fail "$identity leaks the format identity wrapper or constant proxy into the runtime path"
	fi
	# Classify the optional out-of-line converter by stable CPO/manipulator
	# components instead of relying on a host c++filt new enough to understand
	# the compiler's current mangling.
	conversion_calls=$(rg -c \
		'callq?[[:space:]].*_ZN7fast_io20print_reserve_define.*14scalar_manip_t' \
		"$body" || true)
	(( conversion_calls <= 1 )) || fail "$identity contains more than one floating conversion call"
	check_scalar_transport "$identity" "$body" "$conversion_calls"
}

(( ${#compilers[@]} != 0 )) || fail 'no Clang 23-or-newer compiler was found'

for compiler in "${compilers[@]}"; do
	version_line=$($compiler --version | head -n 1)
	name=$(printf '%s' "$version_line" | tr -cs '[:alnum:]' '_')
	for standard in c++20 c++26; do
		assembly="$work_dir/$name.$standard.s"
		object="$work_dir/$name.$standard.o"
		compile_log="$work_dir/$name.$standard.log"
		# C++20 protects the library's minimum language contract.  C++26 exactly
		# reproduces the mode in which the Clang-trunk forwarding regression was
		# reported; checking only the older mode would allow it to return unnoticed.
		if ! "$compiler" -I"$root/include" -std="$standard" -O3 -DNDEBUG \
			-fno-exceptions -fno-rtti -fno-stack-protector -fcf-protection=none \
			-fno-asynchronous-unwind-tables -fno-unwind-tables -S "$source_file" \
			-o "$assembly" >"$compile_log" 2>&1; then
			fail "$version_line -std=$standard failed to compile the probe (see $compile_log)"
		fi
		if ! "$compiler" -I"$root/include" -std="$standard" -O3 -DNDEBUG \
			-fno-exceptions -fno-rtti -fno-stack-protector -fcf-protection=none \
			-fno-asynchronous-unwind-tables -fno-unwind-tables -ffunction-sections \
			-c "$source_file" -o "$object" >>"$compile_log" 2>&1; then
			fail "$version_line -std=$standard failed to compile the ELF size probe (see $compile_log)"
		fi

		for symbol in \
			fast_io_fmt_plain_constant_float \
			fast_io_fmt_plain_constant_double \
			fast_io_fmt_plain_prefixed_constant_double; do
			body="$work_dir/$name.$standard.$symbol"
			extract_function "$assembly" "$symbol" "$body"
			check_constant_leaf "$version_line -std=$standard $symbol" \
				"$body" "$object" "$symbol"
		done

		for symbol in fast_io_fmt_plain_runtime_float fast_io_fmt_plain_runtime_double; do
			body="$work_dir/$name.$standard.$symbol"
			extract_function "$assembly" "$symbol" "$body"
			check_runtime_leaf "$version_line -std=$standard $symbol" "$body"
		done

		printf 'plain floating lowering asm gate: PASS (%s, -std=%s)\n' \
			"$version_line" "$standard"
	done
done
