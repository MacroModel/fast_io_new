#!/usr/bin/env bash
set -euo pipefail

root=${ROOT:-$(cd "$(dirname "$0")/../.." && pwd)}
include_dir="$root/include"
central="$include_dir/fast_io_dsal/impl/misc/push_macros.h"
pop="$include_dir/fast_io_dsal/impl/misc/pop_macros.h"

fail_matches()
{
	local description=$1
	local pattern=$2
	local matches
	matches=$(grep -R -n -E --include='*.h' "$pattern" "$include_dir" | \
		grep -F -v "$central" || true)
	if [[ -n $matches ]]; then
		printf 'C++20 feature source gate: %s\n%s\n' "$description" "$matches" >&2
		exit 1
	fi
}

# All syntax selection belongs to push_macros.h. Keeping call sites free of
# feature tests ensures that the C++20 fallback cannot be accidentally removed.
fail_matches 'raw if consteval syntax outside the central gate' \
	'(^|[^[:alnum:]_])if[[:space:]]+!?consteval([^[:alnum:]_]|$)'
fail_matches 'raw standard assume syntax outside the central gate' \
	'\[\[assume[[:space:]]*\('
fail_matches 'raw builtin assume outside the central gate' \
	'__builtin_assume[[:space:]]*\('
fail_matches 'call-site assume capability test outside the central gate' \
	'#[[:space:]]*(if|elif).*(__has_cpp_attribute|FAST_IO_HAS_ATTRIBUTE)[[:space:]]*\(assume\)'
fail_matches 'static call-operator capability test outside the central gate' \
	'__cpp_static_call_operator'

for macro in FAST_IO_HAS_ATTRIBUTE \
	FAST_IO_HAS_STATIC_CALL_OPERATOR_IN_LANGUAGE_MODE \
	FAST_IO_IF_CONSTEVAL FAST_IO_IF_NOT_CONSTEVAL FAST_IO_ASSUME; do
	push_count=$(grep -F -c "#pragma push_macro(\"$macro\")" "$central")
	pop_count=$(grep -F -c "#pragma pop_macro(\"$macro\")" "$pop")
	if [[ $push_count != 1 || $pop_count != 1 ]]; then
		printf 'C++20 feature source gate: asymmetric %s push=%s pop=%s\n' \
			"$macro" "$push_count" "$pop_count" >&2
		exit 1
	fi
done

printf 'C++20 feature source gate: PASS\n'
