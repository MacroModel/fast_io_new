#!/usr/bin/env bash

set -euo pipefail

root=$(cd -- "$(dirname -- "$0")/../.." && pwd)
print_header="$root/include/fast_io_core_impl/operations/printimpl/print_freestanding_cxx20.h"

# Print must consume only the destination-neutral capability; even a comment-level dependency tends to hide a later
# accidental use of concat's policy CPO during review.
if rg -n 'concat' "$print_header"; then
	echo 'neutral materialization layering: print header contains a concat token' >&2
	exit 1
fi

# This development API has one spelling. Any historical source CPO in the
# implementation would silently recreate a second recognition protocol.
if rg -n 'concat_single_pass_bounded_materialization_(preferred|size)' "$root/include"; then
	echo 'neutral materialization layering: historical source CPO is still present' >&2
	exit 1
fi

echo 'neutral materialization layering: PASS'
