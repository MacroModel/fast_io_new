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

for version in 11 12 13 14 15 16; do
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
		[[ -n $major && $major -ge 11 && $major -le 16 ]] || continue
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
	default_float_body="$work_dir/$name.default_float"
	constant_float_body="$work_dir/$name.constant_float"
	static_body="$work_dir/$name.static"
	raw_static_body="$work_dir/$name.raw_static"
	raw_two_body="$work_dir/$name.raw_two"
	raw_named_two_body="$work_dir/$name.raw_named_two"
	raw_mixed_body="$work_dir/$name.raw_mixed"
	timestamp_body="$work_dir/$name.timestamp"
	runtime_body="$work_dir/$name.runtime"
	runtime_float_body="$work_dir/$name.runtime_float"
	runtime_timestamp_body="$work_dir/$name.runtime_timestamp"
	extract_function "$assembly" fast_io_compiler_constant_default_fmt_int32 "$default_body"
	extract_function "$assembly" fast_io_compiler_constant_posix_fmt_int32 "$constant_body"
	extract_function "$assembly" fast_io_compiler_constant_default_fmt_float64 "$default_float_body"
	extract_function "$assembly" fast_io_compiler_constant_posix_fmt_float64 "$constant_float_body"
	extract_function "$assembly" fast_io_static_argument_posix_fmt_int32 "$static_body"
	extract_function "$assembly" fast_io_static_argument_posix_raw_int32 "$raw_static_body"
	extract_function "$assembly" fast_io_static_argument_posix_raw_two_texts "$raw_two_body"
	extract_function "$assembly" fast_io_static_argument_posix_raw_two_named_texts "$raw_named_two_body"
	extract_function "$assembly" fast_io_static_argument_posix_raw_mixed "$raw_mixed_body"
	extract_function "$assembly" fast_io_compiler_constant_posix_timestamp "$timestamp_body"
	extract_function "$assembly" fast_io_compiler_constant_posix_fmt_runtime_int "$runtime_body"
	extract_function "$assembly" fast_io_compiler_constant_posix_fmt_runtime_float "$runtime_float_body"
	extract_function "$assembly" fast_io_compiler_constant_posix_runtime_timestamp "$runtime_timestamp_body"

	# GCC 11 and 12 cannot keep this value visible through the complete default-C-stream
	# lowering chain without force-inlining the operation-pack implementation. That wider
	# candidate made the optimizer-unknown mixed record 39--42% slower in an 80-pair ABBA
	# run, so the surgical production policy deliberately starts at GCC 13. This is an
	# explicit expected-negative assertion, not a skipped compiler: the two releases must
	# retain the established locked runtime writer until a narrower optimization is proved.
	default_facade_status=constant-folded
	if [[ $family == gcc && $major -lt 13 ]]; then
		default_facade_status=expected-runtime-gcc11-12
		if ! rg -q 'jeaiii|print_reserve_integral_define|_ZN7fast_io2io5print|lower_format_program' "$default_body"; then
			fail "$version_line unexpectedly changed the documented GCC 11/12 default-facade baseline"
		fi
		if rg -q '\$(540876905|12851)|\$0x(203[dD]2069|3233)' "$default_body"; then
			fail "$version_line partially materializes the spelling despite taking the expected runtime fallback"
		fi
	else
		if rg -q 'jeaiii|print_reserve_integral_define|_ZN7fast_io2io5print|lower_format_program' "$default_body"; then
			fail "$version_line routes default fmt literal 32 through a runtime integer writer"
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
	fi
	if [[ $family == gcc && $major -lt 13 ]]; then
		if ! rg -q 'lower_format_program|_ZN7fast_io2io5print|floating_|print_reserve_define' "$default_float_body"; then
			fail "$version_line unexpectedly changed the documented GCC 11/12 default-float baseline"
		fi
	else
		if rg -q 'lower_format_program|_ZN7fast_io2io5print|floating_|print_reserve_define|print_compiler_constant_pre_normalization_static_emit' "$default_float_body"; then
			fail "$version_line routes default fmt literal 3.14 through a runtime formatter"
		fi
		if ! rg -q 'flockfile' "$default_float_body"; then
			fail "$version_line no longer locks the folded default floating facade"
		fi
	fi
	if rg -q 'jeaiii|print_reserve_integral_(compiler_constant_)?define' "$constant_body"; then
		fail "$version_line routes explicit fmt literal 32 through an integer writer"
	fi
	if rg -q 'lower_format_program|_ZN7fast_io2io5print' "$constant_body"; then
		fail "$version_line leaves explicit fmt literal 32 behind a lowering call"
	fi
	if rg -q 'lower_format_program|_ZN7fast_io2io5print|floating_|print_reserve_define|print_compiler_constant_pre_normalization_static_emit' "$constant_float_body"; then
		fail "$version_line routes explicit fmt literal 3.14 through a formatter call"
	fi
	if rg -q 'writev|scatter_write' "$constant_float_body"; then
		fail "$version_line routes the explicit floating record through scatter output"
	fi
	if [[ $(rg -c 'syscall' "$constant_float_body") -ne 1 ]]; then
		fail "$version_line explicit floating record does not contain exactly one scalar syscall"
	fi

	# A plain argument value is not part of the function-template specialization, so C++ cannot use it to initialize a
	# value-dependent static object.  The complete short spelling must instead be one caller-local core array sent by
	# one scalar write.  It must not retain the old integer writer or build automatic iovec metadata.
	if rg -q 'writev|scatter_write|print_static_scatter' "$constant_body"; then
		fail "$version_line routes the explicit short record through scatter output"
	fi
	if ! rg -q 'rsp|esp' "$constant_body"; then
		fail "$version_line did not materialize the plain constant in its caller-local record"
	fi
	if [[ $(rg -c 'syscall' "$constant_body") -ne 1 ]]; then
		fail "$version_line explicit short record does not contain exactly one scalar syscall"
	fi

	# static_arg places the value in the type graph. This is the language-level
	# case in which the IO planner can own one merged provider in rodata and pass
	# its address directly to write-all, without a stack copy.
	if rg -q 'jeaiii|print_reserve_integral_(compiler_constant_)?define|writev|scatter_write' "$static_body"; then
		fail "$version_line routes static_arg<32> through formatting or scatter output"
	fi
	if rg -q 'rsp|esp|[[:space:]]push[[:space:]]|[[:space:]]pop[[:space:]]' "$static_body"; then
		fail "$version_line copies static_arg<32> through automatic storage"
	fi
	if [[ $(rg -c 'syscall' "$static_body") -ne 1 ]]; then
		fail "$version_line static_arg<32> does not contain exactly one scalar syscall"
	fi
	if ! rg -q 'print_static_provider_merged_run_provider.*storage' "$static_body"; then
		fail "$version_line lost the IO-owned merged static provider"
	fi

	# Raw IO owns the same facility in the core manipulator graph.  A singleton,
	# an adjacent pair, and a pair of named nodes must each become one provider-
	# owned core freestanding array and one scalar write, with no format layer,
	# integer conversion, automatic payload copy, or writev metadata.
	for raw_body in "$raw_static_body" "$raw_two_body" "$raw_named_two_body"; do
		if rg -q 'compiled_static_replacement_provider|jeaiii|print_reserve_integral_(compiler_constant_)?define|writev|scatter_write' "$raw_body"; then
			fail "$version_line routes a raw static-argument record through format/conversion/scatter output"
		fi
		if rg -q 'rsp|esp|[[:space:]]push[[:space:]]|[[:space:]]pop[[:space:]]' "$raw_body"; then
			fail "$version_line copies a raw static-argument record through automatic storage"
		fi
		if [[ $(rg -c 'syscall' "$raw_body") -ne 1 ]]; then
			fail "$version_line raw static-argument record does not contain exactly one scalar syscall"
		fi
	done
	if ! rg -q 'static_provider_storage_t.*static_argument_native_provider.*storage' "$raw_static_body"; then
		fail "$version_line lost the singleton core static-argument provider"
	fi
	for raw_body in "$raw_two_body" "$raw_named_two_body"; do
		if ! rg -q 'print_static_provider_merged_run_provider.*storage' "$raw_body"; then
			fail "$version_line lost a core merged static-argument provider"
		fi
	done

	# A mixed run cannot be merged because the integer is optimizer-unknown.  Its
	# immutable component must nevertheless remain a provider pointer instead of
	# being copied into the runtime digit scratch area.
	if ! rg -q 'static_provider_storage_t.*static_argument_native_provider.*storage' "$raw_mixed_body"; then
		fail "$version_line lost the provider pointer in a mixed raw static/runtime run"
	fi
	if rg -q 'print_static_provider_merged_run_provider.*storage' "$raw_mixed_body"; then
		fail "$version_line incorrectly treats a mixed raw run as an all-static provider"
	fi
	symbol_table="$work_dir/$name.symbols"
	objdump -tC "$object" > "$symbol_table"
	# ELF section names for structural NTTPs can exceed readelf's displayed
	# field width. The demangled object symbol table proves both the owning
	# provider type and its actual read-only section without truncation.
	if ! rg -q '\.rodata.*print_static_provider_merged_run_provider.*::storage$' "$symbol_table"; then
		fail "$version_line did not place merged static records in rodata"
	fi
	if ! rg -q '\.rodata.*static_provider_storage_t.*::storage$' "$symbol_table"; then
		fail "$version_line did not place singleton provider records in rodata"
	fi

	# A fixed timestamp is a two-field scalar aggregate.  Its source-safe CPO must run before the ordinary normalization
	# bridge is allowed to outline, otherwise GCC and Clang lose __builtin_constant_p visibility at that call boundary.
	if rg -q 'jeaiii|print_reserve_integral_define|[[:space:]]call[q]?[[:space:]]|writev|scatter_write' "$timestamp_body"; then
		fail "$version_line failed to fold the fixed timestamp at the public source boundary"
	fi
	if [[ $(rg -c 'syscall' "$timestamp_body") -ne 1 ]]; then
		fail "$version_line fixed timestamp does not contain exactly one scalar syscall"
	fi

	# An optimizer-unknown value must retain the historical dynamic writer and must not acquire a reference to the
	# compiler-constant digit provider.  This is the zero-residue half of the value gate.
	if rg -q 'compiler_constant_integral_(pair|digit)_fragments|compiler_constant_scalar_manip' "$runtime_body"; then
		fail "$version_line leaks compiler-constant integer state into the runtime-value facade"
	fi
	if rg -q 'compiler_constant_floating|compiler_constant_scalar_manip' "$runtime_float_body"; then
		fail "$version_line leaks compiler-constant floating state into the runtime-value facade"
	fi
	if rg -q 'compiler_constant_timestamp|print_reserve_integral_compiler_constant' "$runtime_timestamp_body"; then
		fail "$version_line leaks compiler-constant timestamp state into the runtime-value facade"
	fi

	printf 'compiler-constant integer facade asm gate: PASS (%s, default facade: %s)\n' \
		"$version_line" "$default_facade_status"
done

(( compiler_count != 0 )) || fail 'no GCC 11-16 or Clang 17-23 compiler was found'
