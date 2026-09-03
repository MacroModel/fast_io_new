#!/bin/sh
set -u
set -f

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
HOST_OS=$(uname -s)
NEW_ROOT=${NEW_ROOT:-"$SCRIPT_DIR/../.."}
OLD_ROOT=${OLD_ROOT:-"$SCRIPT_DIR/../../../fast_io"}
DARWIN_CXX=/Users/liyinan/Documents/MacroModel/tool-chain/tools/aarch64-apple-darwin-llvm/llvm/bin/clang++
DARWIN_SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
if [ "$HOST_OS" = Darwin ]; then
	BUILD_DIR=${BUILD_DIR:-"/tmp/fast_io_state_machine_cpo.$$"}
	CXX=${CXX:-"$DARWIN_CXX"}
	CXXFLAGS=${CXXFLAGS:---sysroot=$DARWIN_SYSROOT -march=native -fuse-ld=lld -O3 -std=c++20 -DNDEBUG}
else
	BUILD_DIR=${BUILD_DIR:-"${TMPDIR:-/tmp}/fast_io_state_machine_cpo.$$"}
	CXX=${CXX:-c++}
	CXXFLAGS=${CXXFLAGS:--O3 -march=native -std=c++20 -DNDEBUG}
fi
SEED=${SEED:-7640891576956012809}
TARGET_MS=${TARGET_MS:-80}
COOLDOWN_SECONDS=${COOLDOWN_SECONDS:-2}
RUN_TIMEOUT_SECONDS=${RUN_TIMEOUT_SECONDS:-0.8}
: "${SCAN_INPUTS:=contiguous chunk1 chunk3 chunk7}"
: "${SCAN_RECEIVERS:=int10 int16 double string}"
: "${TO_OPERATIONS:=text-int text-double scalar-string inplace-int}"
# This is an axis-covering narrow matrix, not an implicit Cartesian product.
# Every entry still owns one translation unit and one old/new ABBA schedule.
: "${TO_PROTOCOL_CELLS:=runtime:f:1:to:builtin
runtime:d:2:inplace:context
runtime:pp:8:to:builtin
runtime:ss:32:inplace:context
runtime:mixed-proof:8:inplace:builtin
literal:literal:1:to:builtin
literal:literal:2:inplace:builtin
literal:literal:8:to:builtin
literal:literal:32:inplace:builtin}"
: "${OLD_SUPPORTS_FLOAT_SCAN:=0}"

if [ ! -d "$NEW_ROOT/include" ] || [ ! -d "$OLD_ROOT/include" ]; then
	printf '%s\n' "NEW_ROOT and OLD_ROOT must each contain include/" >&2
	exit 2
fi
NEW_ROOT=$(CDPATH= cd -- "$NEW_ROOT" && pwd -P)
OLD_ROOT=$(CDPATH= cd -- "$OLD_ROOT" && pwd -P)

case $TARGET_MS in
	'' | *[!0-9]*)
		printf '%s\n' "TARGET_MS must be an integer in [20,80]." >&2
		exit 2
		;;
esac
if [ "$TARGET_MS" -lt 20 ] || [ "$TARGET_MS" -gt 80 ]; then
	printf '%s\n' "TARGET_MS must be an integer in [20,80]." >&2
	exit 2
fi

if [ "$HOST_OS" = Linux ] && [ -z "${CPU:-}" ] &&
	[ "${ALLOW_UNPINNED:-0}" != 1 ]; then
	printf '%s\n' \
		"Set CPU to one verified idle P-core (or ALLOW_UNPINNED=1 for a non-timing smoke test)." >&2
	exit 2
fi

if [ "$HOST_OS" = Darwin ]; then
	case $BUILD_DIR in
		/tmp/*) ;;
		*)
			printf '%s\n' "On Darwin, BUILD_DIR must be a literal path below /tmp/." >&2
			exit 2
			;;
	esac
	if [ "$CXX" != "$DARWIN_CXX" ]; then
		printf '%s\n' "On Darwin, CXX must be $DARWIN_CXX." >&2
		exit 2
	fi
	case " $CXXFLAGS " in
		*" --sysroot=$DARWIN_SYSROOT "*) ;;
		*)
			printf '%s\n' "On Darwin, CXXFLAGS must contain --sysroot=$DARWIN_SYSROOT." >&2
			exit 2
			;;
	esac
	case " $CXXFLAGS " in
		*" -march=native "*) ;;
		*)
			printf '%s\n' "On Darwin, CXXFLAGS must contain -march=native." >&2
			exit 2
			;;
	esac
	case " $CXXFLAGS " in
		*" -fuse-ld=lld "*) ;;
		*)
			printf '%s\n' "On Darwin, CXXFLAGS must contain -fuse-ld=lld." >&2
			exit 2
			;;
	esac
	if [ -n "${CPU:-}" ]; then
		printf '%s\n' "CPU affinity is Linux-only; leave CPU unset on Darwin." >&2
		exit 2
	fi
	if [ ! -x /usr/bin/perl ]; then
		printf '%s\n' "Darwin timing requires /usr/bin/perl for the single-process sub-second deadline." >&2
		exit 2
	fi
fi

if [ ! -x "$CXX" ] && ! command -v "$CXX" >/dev/null 2>&1; then
	printf '%s\n' "CXX is not executable: $CXX" >&2
	exit 2
fi
if ! compiler_version_output=$("$CXX" --version 2>&1); then
	printf '%s\n' "Unable to query compiler version: $CXX" >&2
	exit 2
fi
COMPILER_VERSION_LINE=$(printf '%s\n' "$compiler_version_output" | sed -n '1p')
COMPILER_DESCRIPTION="$CXX | $COMPILER_VERSION_LINE"

run_binary()
{
	if [ "$HOST_OS" = Linux ]; then
		# The outer deadline covers corpus generation, oracle preflight, pilot, and
		# measurement. It is independent of the in-process 800 ms guard.
		if [ -n "${CPU:-}" ]; then
			timeout --signal=KILL "${RUN_TIMEOUT_SECONDS}s" \
				taskset -c "$CPU" "$1" "$SEED" "$TARGET_MS"
		else
			timeout --signal=KILL "${RUN_TIMEOUT_SECONDS}s" \
				"$1" "$SEED" "$TARGET_MS"
		fi
	else
		# Time::HiRes arms ITIMER_REAL and then `exec` replaces Perl with the
		# benchmark. Thus Darwin gets a sub-second outer deadline without a
		# concurrent watchdog process, preserving the M4 one-task invariant.
		/usr/bin/perl -MTime::HiRes=ualarm -e \
			'my $seconds = shift; ualarm(int($seconds * 1000000)); exec @ARGV; die "exec failed: $!\n";' \
			"$RUN_TIMEOUT_SECONDS" "$1" "$SEED" "$TARGET_MS"
	fi
}

prepare_build_output()
{
	output=$1
	case $output in
		"$BUILD_DIR"/*)
			# Removing only the selected artifact makes a failed rebuild observable;
			# it cannot silently leave a binary from another root or flag set runnable.
			rm -f -- "$output"
			;;
		*)
			printf '%s\n' "refusing to replace an artifact outside BUILD_DIR: $output" >&2
			exit 2
			;;
	esac
}

serial_make()
{
	# Empty inherited jobserver settings plus command-line -j1 make the Darwin
	# one-task rule and the Linux no-compile-during-timing rule explicit.
	MAKEFLAGS= MFLAGS= make -B -s --no-print-directory -j1 -C "$SCRIPT_DIR" "$@"
}

build_scan()
{
	root=$1
	tag=$2
	input=$3
	receiver=$4
	prepare_build_output "$BUILD_DIR/$tag/scan-$input-$receiver" || return 1
	serial_make scan CXX="$CXX" CXXFLAGS="$CXXFLAGS" FAST_IO_ROOT="$root" \
		BUILD_DIR="$BUILD_DIR" TAG="$tag" INPUT="$input" RECEIVER="$receiver"
}

build_to()
{
	root=$1
	tag=$2
	operation=$3
	prepare_build_output "$BUILD_DIR/$tag/to-$operation" || return 1
	serial_make to CXX="$CXX" CXXFLAGS="$CXXFLAGS" FAST_IO_ROOT="$root" \
		BUILD_DIR="$BUILD_DIR" TAG="$tag" TO="$operation"
}

build_transcoder()
{
	mode=$1
	prepare_build_output "$BUILD_DIR/new/transcoder-$mode" || return 1
	serial_make transcoder CXX="$CXX" CXXFLAGS="$CXXFLAGS" \
		FAST_IO_ROOT="$NEW_ROOT" BUILD_DIR="$BUILD_DIR" TAG=new \
		TRANSCODER="$mode"
}

build_to_protocol_cell()
{
	cell_spec=$1
	old_ifs=$IFS
	IFS=:
	set -- $cell_spec
	IFS=$old_ifs
	if [ "$#" -ne 5 ]; then
		printf '%s\n' \
			"invalid TO_PROTOCOL_CELLS entry '$cell_spec': expected mode:source:pack:frontdoor:target" >&2
		exit 2
	fi
	mode=$1
	source=$2
	pack=$3
	frontdoor=$4
	target=$5
	binary_suffix="to-protocol-$mode-$source-p$pack-$frontdoor-$target"
	for variant in old new; do
		if [ "$variant" = old ]; then
			root=$OLD_ROOT
		else
			root=$NEW_ROOT
		fi
		if ! prepare_build_output "$BUILD_DIR/$variant/$binary_suffix"; then
			printf '%s\n' "unable to prepare: $variant/$binary_suffix" >&2
			continue
		fi
		if ! serial_make to-protocol CXX="$CXX" CXXFLAGS="$CXXFLAGS" \
			FAST_IO_ROOT="$root" BUILD_DIR="$BUILD_DIR" TAG="$variant" \
			TO_PROTOCOL_MODE="$mode" TO_PROTOCOL_SOURCE="$source" \
			TO_PROTOCOL_PACK="$pack" TO_PROTOCOL_FRONTDOOR="$frontdoor" \
			TO_PROTOCOL_TARGET="$target"; then
			printf '%s\n' "build failed: $variant/$binary_suffix" >&2
		fi
	done
}

try_build()
{
	description=$1
	shift
	if ! "$@"; then
		printf '%s\n' "build failed: $description" >&2
	fi
}

csv_field()
{
	escaped=$(printf '%s' "$1" | sed 's/"/""/g')
	printf '"%s"' "$escaped"
}

csv_row()
{
	separator=
	for field do
		printf '%s' "$separator"
		csv_field "$field"
		separator=,
	done
	printf '\n'
}

FAILURE_COUNT=0
emit_execution()
{
	case_name=$1
	variant=$2
	order=$3
	repeat=$4
	root=$5
	binary=$6
	if [ ! -x "$binary" ]; then
		status=build-failed
		raw_result="fresh executable is absent: $binary"
		FAILURE_COUNT=$((FAILURE_COUNT + 1))
	elif raw_result=$(run_binary "$binary" 2>&1); then
		status=ok
	else
		exit_status=$?
		case $exit_status in
			124 | 137 | 142)
				status=timeout
				;;
			*)
				status="exit-$exit_status"
				;;
		esac
		if [ -z "$raw_result" ]; then
			raw_result="process exited with status $exit_status"
		fi
		FAILURE_COUNT=$((FAILURE_COUNT + 1))
	fi
	csv_row "$case_name" "$variant" "$order" "$repeat" "$SEED" \
		"${TARGET_MS}ms" "$status" "$COMPILER_DESCRIPTION" "$CXXFLAGS" \
		"$root" "$raw_result"
}

emit_skip()
{
	case_name=$1
	variant=$2
	order=$3
	repeat=$4
	root=$5
	reason=$6
	csv_row "$case_name" "$variant" "$order" "$repeat" "$SEED" \
		"${TARGET_MS}ms" skip "$COMPILER_DESCRIPTION" "$CXXFLAGS" \
		"$root" "$reason"
}

run_old_new_abba()
{
	case_name=$1
	old_binary=$2
	new_binary=$3
	emit_execution "$case_name" old 1 1 "$OLD_ROOT" "$old_binary"
	emit_execution "$case_name" new 2 1 "$NEW_ROOT" "$new_binary"
	emit_execution "$case_name" new 3 2 "$NEW_ROOT" "$new_binary"
	emit_execution "$case_name" old 4 2 "$OLD_ROOT" "$old_binary"
}

run_old_skip_new_abba()
{
	case_name=$1
	new_binary=$2
	reason=$3
	emit_skip "$case_name" old 1 1 "$OLD_ROOT" "$reason"
	emit_execution "$case_name" new 2 1 "$NEW_ROOT" "$new_binary"
	emit_execution "$case_name" new 3 2 "$NEW_ROOT" "$new_binary"
	emit_skip "$case_name" old 4 2 "$OLD_ROOT" "$reason"
}

run_new_control_abba()
{
	case_name=$1
	first_variant=$2
	first_binary=$3
	second_variant=$4
	second_binary=$5
	emit_execution "$case_name" "$first_variant" 1 1 "$NEW_ROOT" "$first_binary"
	emit_execution "$case_name" "$second_variant" 2 1 "$NEW_ROOT" "$second_binary"
	emit_execution "$case_name" "$second_variant" 3 2 "$NEW_ROOT" "$second_binary"
	emit_execution "$case_name" "$first_variant" 4 2 "$NEW_ROOT" "$first_binary"
}

# Complete every forced serial compiler invocation before any timed process.
# Failed cells are retained in the fixed CSV schema as build-failed rows rather
# than allowing `set -e` or an old artifact to truncate or corrupt the matrix.
for input in $SCAN_INPUTS; do
	for receiver in $SCAN_RECEIVERS; do
		try_build "new/scan-$input-$receiver" \
			build_scan "$NEW_ROOT" new "$input" "$receiver"
		if [ "$receiver" != double ] || [ "$OLD_SUPPORTS_FLOAT_SCAN" = 1 ]; then
			try_build "old/scan-$input-$receiver" \
				build_scan "$OLD_ROOT" old "$input" "$receiver"
		fi
	done
done
for operation in $TO_OPERATIONS; do
	try_build "new/to-$operation" build_to "$NEW_ROOT" new "$operation"
	if [ "$operation" != text-double ] || [ "$OLD_SUPPORTS_FLOAT_SCAN" = 1 ]; then
		try_build "old/to-$operation" build_to "$OLD_ROOT" old "$operation"
	fi
done
for cell in $TO_PROTOCOL_CELLS; do
	build_to_protocol_cell "$cell"
done
for mode in adapter staged; do
	try_build "new/transcoder-$mode" build_transcoder "$mode"
done

if [ "$COOLDOWN_SECONDS" != 0 ]; then
	sleep "$COOLDOWN_SECONDS"
fi

csv_row case variant order repeat seed target status compiler flags root raw_result
for input in $SCAN_INPUTS; do
	for receiver in $SCAN_RECEIVERS; do
		case_name="scan:$input:$receiver"
		if [ "$receiver" = double ] && [ "$OLD_SUPPORTS_FLOAT_SCAN" != 1 ]; then
			run_old_skip_new_abba "$case_name" \
				"$BUILD_DIR/new/scan-$input-$receiver" no-floating-scan-cpo
		else
			run_old_new_abba "$case_name" \
				"$BUILD_DIR/old/scan-$input-$receiver" \
				"$BUILD_DIR/new/scan-$input-$receiver"
		fi
	done
done
for operation in $TO_OPERATIONS; do
	case_name="to:$operation"
	if [ "$operation" = text-double ] && [ "$OLD_SUPPORTS_FLOAT_SCAN" != 1 ]; then
		run_old_skip_new_abba "$case_name" \
			"$BUILD_DIR/new/to-$operation" no-floating-scan-cpo
	else
		run_old_new_abba "$case_name" "$BUILD_DIR/old/to-$operation" \
			"$BUILD_DIR/new/to-$operation"
	fi
done

for cell in $TO_PROTOCOL_CELLS; do
	old_ifs=$IFS
	IFS=:
	set -- $cell
	IFS=$old_ifs
	if [ "$#" -ne 5 ]; then
		printf '%s\n' \
			"invalid TO_PROTOCOL_CELLS entry '$cell': expected mode:source:pack:frontdoor:target" >&2
		exit 2
	fi
	mode=$1
	source=$2
	pack=$3
	frontdoor=$4
	target=$5
	binary_suffix="to-protocol-$mode-$source-p$pack-$frontdoor-$target"
	run_old_new_abba "to-protocol:$mode:$source:p$pack:$frontdoor:$target" \
		"$BUILD_DIR/old/$binary_suffix" "$BUILD_DIR/new/$binary_suffix"
done

# The adapter API is new-only. Its staged control uses the same A-B-B-A order
# so first/last drift remains visible without mislabelling either row as old.
run_new_control_abba transcoder:adapter-vs-staged new-adapter \
	"$BUILD_DIR/new/transcoder-adapter" new-staged \
	"$BUILD_DIR/new/transcoder-staged"

printf 'artifacts,%s\n' "$BUILD_DIR" >&2
if [ "$FAILURE_COUNT" -ne 0 ]; then
	printf '%s\n' "$FAILURE_COUNT benchmark execution(s) failed; all scheduled rows were recorded." >&2
	exit 1
fi
