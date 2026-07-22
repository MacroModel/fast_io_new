#!/usr/bin/env bash

set -euo pipefail

deps_root="${FAST_IO_JSON_BENCH_DEPS:-/tmp/fast_io_json_deps}"
mkdir -p "${deps_root}"

clone_at() {
	local name="$1"
	local repository="$2"
	local commit="$3"
	local destination="${deps_root}/${name}"

	if [[ ! -d "${destination}/.git" ]]; then
		git clone --filter=blob:none --no-checkout "${repository}" "${destination}"
	fi
	git -C "${destination}" fetch --depth=1 origin "${commit}"
	# Refuse to overwrite local work in an existing dependency checkout.
	git -C "${destination}" checkout --detach "${commit}"
}

# Keep comparisons reproducible. Update these hashes intentionally and record
# the change in README.md when refreshing the benchmark baseline.
clone_at yyjson https://github.com/ibireme/yyjson.git ac8f6074e1fbc43ec496aa1404b460d08b55d7a5
clone_at rapidjson https://github.com/Tencent/rapidjson.git 24b5e7a8b27f42fa16b96fc70aade9106cf7102f
clone_at simdjson https://github.com/simdjson/simdjson.git 8e6bac94877f2d3d026000d36ce81e0aaf38d26f
clone_at glaze https://github.com/stephenberry/glaze.git 3a603326da390da29aa5aab2252d3e64798c5567
