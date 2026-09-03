#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
NEW_ROOT=${NEW_ROOT:-"$SCRIPT_DIR/../.."}
OLD_ROOT=${OLD_ROOT:-"$SCRIPT_DIR/../../../fast_io"}
BUILD_DIR=${BUILD_DIR:-"${TMPDIR:-/tmp}/fast_io_cpo_matrix.$$"}
CXX=${CXX:-c++}
SEED=${SEED:-7640891576956012809}
TARGET_MS=${TARGET_MS:-150}
COOLDOWN_SECONDS=${COOLDOWN_SECONDS:-2}
LINE=${LINE:-0}
: "${SOURCES:=f d p s bs a mixed mixed_b}"
: "${PACKS:=1 2 8 32}"
: "${PRINT_OUTPUTS:=obuffer raw}"
: "${CONCAT_RESULTS:=std fast}"

if [ ! -d "$NEW_ROOT/include" ] || [ ! -d "$OLD_ROOT/include" ]; then
	printf '%s\n' "NEW_ROOT and OLD_ROOT must each contain include/" >&2
	exit 2
fi

if [ "$(uname -s)" = Linux ] && [ -z "${CPU:-}" ] &&
	[ "${ALLOW_UNPINNED:-0}" != 1 ]; then
	printf '%s\n' \
		"Set CPU to one verified idle P-core (or ALLOW_UNPINNED=1 for non-timing smoke tests)." >&2
	exit 2
fi

run_binary()
{
	if [ -n "${CPU:-}" ]; then
		taskset -c "$CPU" "$1" "$SEED" "$TARGET_MS"
	else
		"$1" "$SEED" "$TARGET_MS"
	fi
}

pair_index=0
run_pair()
{
	new_binary=$1
	old_binary=$2
	if [ $((pair_index % 2)) -eq 0 ]; then
		old_line=$(run_binary "$old_binary")
		printf 'old,%s\n' "$old_line"
		new_line=$(run_binary "$new_binary")
		printf 'new,%s\n' "$new_line"
	else
		new_line=$(run_binary "$new_binary")
		printf 'new,%s\n' "$new_line"
		old_line=$(run_binary "$old_binary")
		printf 'old,%s\n' "$old_line"
	fi
	pair_index=$((pair_index + 1))
}

build_print()
{
	root=$1
	tag=$2
	source=$3
	pack=$4
	output=$5
	make -s --no-print-directory -j1 -C "$SCRIPT_DIR" print \
		CXX="$CXX" FAST_IO_ROOT="$root" BUILD_DIR="$BUILD_DIR" TAG="$tag" \
		SOURCE="$source" PACK="$pack" LINE="$LINE" OUTPUT="$output"
}

build_concat()
{
	root=$1
	tag=$2
	source=$3
	pack=$4
	result=$5
	make -s --no-print-directory -j1 -C "$SCRIPT_DIR" concat \
		CXX="$CXX" FAST_IO_ROOT="$root" BUILD_DIR="$BUILD_DIR" TAG="$tag" \
		SOURCE="$source" PACK="$pack" LINE="$LINE" RESULT="$result"
}

# Complete the serial build phase before starting any timed process.  Mixing a
# compiler invocation between old/new samples would introduce a systematic
# thermal and scheduler-state bias that alternating execution order cannot fix.
for source in $SOURCES; do
	for pack in $PACKS; do
		for output in $PRINT_OUTPUTS; do
			build_print "$NEW_ROOT" new "$source" "$pack" "$output"
			build_print "$OLD_ROOT" old "$source" "$pack" "$output"
		done
		for result in $CONCAT_RESULTS; do
			build_concat "$NEW_ROOT" new "$source" "$pack" "$result"
			build_concat "$OLD_ROOT" old "$source" "$pack" "$result"
		done
	done
done

if [ "$COOLDOWN_SECONDS" != 0 ]; then
	sleep "$COOLDOWN_SECONDS"
fi

printf '%s\n' \
	"variant,operation,source,pack,line,destination,seed,iterations,seconds,ns_per_call,checksum,validation_digest,timed_digest"

for source in $SOURCES; do
	for pack in $PACKS; do
		for output in $PRINT_OUTPUTS; do
			new_binary="$BUILD_DIR/new/print-$source-n$pack-l$LINE-$output"
			old_binary="$BUILD_DIR/old/print-$source-n$pack-l$LINE-$output"
			run_pair "$new_binary" "$old_binary"
		done
		for result in $CONCAT_RESULTS; do
			new_binary="$BUILD_DIR/new/concat-$source-n$pack-l$LINE-$result"
			old_binary="$BUILD_DIR/old/concat-$source-n$pack-l$LINE-$result"
			run_pair "$new_binary" "$old_binary"
		done
	done
done

printf 'artifacts,%s\n' "$BUILD_DIR" >&2
