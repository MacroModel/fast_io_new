# JSON DOM benchmark

This benchmark compares DOM parsing and compact DOM serialization for
fast_io/bizwen JSON, yyjson, RapidJSON, simdjson, and Glaze. It generates one
identical UTF-8 corpus before timing, validates each implementation outside the
timed region, performs warm-up rounds, and reports median throughput through
fast_io output.

The comparison is built as C++23 because the pinned Glaze revision requires
it. The fast_io JSON driver itself remains C++20. Dependencies and binaries are
kept outside the repository under `/tmp` by default.

```bash
cd benchmark/0023.json
make fetch
make -j
make run CPU=4
```

`CPU=4` is only a default suitable for the development machine. Select an
online performance core for the machine running the benchmark.

Pinned revisions:

- yyjson: `ac8f6074e1fbc43ec496aa1404b460d08b55d7a5`
- RapidJSON: `24b5e7a8b27f42fa16b96fc70aade9106cf7102f`
- simdjson: `8e6bac94877f2d3d026000d36ce81e0aaf38d26f`
- Glaze: `3a603326da390da29aa5aab2252d3e64798c5567`

