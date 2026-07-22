# JSON DOM benchmark

This benchmark compares DOM parsing and compact DOM serialization for
fast_io/json, yyjson, RapidJSON, simdjson, and Glaze. It generates one
identical UTF-8 corpus before timing, parses every serialized result with
yyjson and checks deep semantic equality outside the timed region, performs
warm-up rounds, and reports median throughput through fast_io output.

Each library adapter is compiled in its own translation unit and exposed to the
common corpus and timing harness through a small opaque interface. This keeps a
large third-party template instantiation from changing another writer's GCC
inlining budget, while retaining identical lifecycle and validation rules.

The table reports both `fast_io::string` and `std::string` for the explicit
`fast_io mutable` and `fast_io immutable` models. Mutable rows walk the live,
fully editable DOM for every serialization; immutable rows use the compact
read-only parse tape and its cached canonical byte count. yyjson is reported twice:
`yyjson mutable` uses `yyjson_mut_doc` and `yyjson_mut_write()`, while
`yyjson immutable` retains the compact parse-only document and
`yyjson_write()`. This prevents an immutable 16-byte tape from being presented
as the performance of a fully editable DOM. Every serializer's returned
storage is created and destroyed inside the timed operation.

The comparison is built as C++23 because the pinned Glaze revision requires
it. The fast_io JSON driver itself remains C++20. Dependencies and binaries are
kept outside the repository under `/tmp` by default.

```bash
cd benchmark/0023.json
make fetch
make -j
make run CPU=4
```

The equivalent opt-in CMake target is
`benchmark.0023.json.json_bench`; configure with
`-DFAST_IO_BUILD_EXTERNAL_JSON_BENCHMARK=ON` and, if needed,
`-DFAST_IO_JSON_BENCH_DEPS=/path/to/dependencies`.

`CPU=4` is only a default suitable for the development machine. Select an
online performance core for the machine running the benchmark.

Pinned revisions:

- yyjson: `ac8f6074e1fbc43ec496aa1404b460d08b55d7a5`
- RapidJSON: `24b5e7a8b27f42fa16b96fc70aade9106cf7102f`
- simdjson: `8e6bac94877f2d3d026000d36ce81e0aaf38d26f`
- Glaze: `3a603326da390da29aa5aab2252d3e64798c5567`
