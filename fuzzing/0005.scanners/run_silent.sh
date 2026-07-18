#!/bin/sh
# libFuzzer writes progress diagnostics even for a healthy run.  This wrapper intentionally exposes only the process
# status so automation cannot mistake libFuzzer's internal progress stream for application output or a test failure.
if [ "$#" -eq 0 ]; then
	exit 64
fi
"$@" >/dev/null 2>&1
status=$?
exit "$status"
