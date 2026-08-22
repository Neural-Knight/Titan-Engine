# low-latency tuning and cross-platform profiling

Same machine as `environment.md` (Apple M5 Pro, macOS, arm64).

## Platform matrix

| Feature | macOS (this machine) | Linux |
|---|---|---|
| `pinCurrentThreadToCpu` / `pinThread` (`titan/platform/thread_affinity.hpp`) | no-op, returns `false` | `pthread_setaffinity_np`, returns `true`/`false` |
| `TITAN_PIN_CONSUMER_CPU` / `TITAN_PIN_PRODUCER_CPU` env vars | read, but pin call is a no-op | pins the named thread to that CPU |
| `perfStatAvailable()` / `perfStatHint()` (`titan/platform/profiling.hpp`) | `false` / generic note | `true` / `perf stat -d <binary>` |
| `tools/run_profiled_bench.sh` | runs the binary directly, prints a note | runs it under `perf stat -d` |
| `TITAN_BENCHMARK_RELEASE=ON` (`-O2 -DNDEBUG` on benchmark libs/binaries) | works (Apple Clang) | works (GCC/Clang) |
| Default build (`TITAN_BENCHMARK_RELEASE` unset) | debug-equivalent, no `-O` flags | debug-equivalent, no `-O` flags |

All Linux-only code paths are guarded by `#ifdef __linux__` with a one-line
`// not available on this platform` stub in the `#else` branch --
`src/platform/thread_affinity.cpp` is the only file with the guard, so there
is exactly one place to look, not several scattered `#ifdef`s.

## Building the Release benchmark profile

```
cmake -S . -B build-rel -DTITAN_BENCHMARK_RELEASE=ON
cmake --build build-rel
```

This applies `-O2` + `NDEBUG` to `titan_reference`, `titan_optimized`,
`titan_exchange`, `titan_book`, `titan_market_data`, `titan_bench`,
`titan_pipeline`, and every `bench_*` binary -- not to `titan_tests` or
`titan_itch`/`titan_replay` (unused by benchmarks). Default
`cmake -S . -B build && cmake --build build` is untouched: the option
defaults `OFF`, so the default path keeps its existing flags.

## Debug (default) vs Release (`TITAN_BENCHMARK_RELEASE=ON`)

Three back-to-back runs per binary per build; range shown since single-shot
runs on a lightly-loaded laptop vary (no repeated-trial averaging beyond
what `bench_match_optimized`'s Google Benchmark harness already does).

### steady_limit_flow, full-replay latency (ns) -- `bench_match_optimized` / `bench_allocations`

| Path | Debug p50 | Debug p99 | Release p50 | Release p99 | Speedup (p50) |
|---|---|---|---|---|---|
| OptimizedMatcher direct (`bench_match_optimized`) | 40,278,400 | 40,914,700 | 4,392,210 | 4,602,330 | ~9.2x |
| ReferenceMatcher direct (`bench_allocations`) | 41,978,500 | 44,546,800 | 4,570,000 | 4,796,170 | ~9.2x |
| OptimizedMatcher direct (`bench_allocations`) | 41,746,400 | 42,287,300 | 4,405,250 | 4,650,380 | ~9.5x |

Allocation counts (`bench_allocations`) are identical in both builds, as
expected -- `-O2` changes code speed, not the number of heap calls the code
makes: ReferenceMatcher 1,337,882 / OptimizedMatcher 1,265,432 allocations
over 200,000 measured ops, matching `allocations.md`.

### steady_limit_flow via `bench_pipeline` (direct replay vs StagedProcessor)

| Metric | Debug (range across 3 runs) | Release (range across 3 runs) |
|---|---|---|
| Direct throughput (ops/sec) | 224,283 -- 296,326 | 1,626,870 -- 2,767,340 |
| Direct p50 latency (ns) | 1,500 -- 2,125 | 167 -- 291 |
| Pipeline throughput (ops/sec) | 275,153 -- 279,902 | 1,946,690 -- 2,691,870 |
| Pipeline enqueue-to-applied p50 (ns) | 13,547,400 -- 14,034,300 | 1,529,460 -- 2,073,710 |

Release is roughly 7-10x faster across the board here, consistent with the
`bench_match_optimized`/`bench_allocations` numbers above -- this is the
same InstrumentRegistry/OrderManager code, just compiled with `-O2` instead
of no `-O` flag at all.

The pipeline's enqueue-to-applied latency is still ~2-4 orders of magnitude
above its own direct-path latency in *both* builds, for the same reason
documented in `pipeline.json`: this benchmark's producer has no
real per-op cost, so it saturates the 4095-slot `SpscQueue` almost
immediately and stays saturated for the whole run -- `-O2` makes the
consumer drain faster in absolute terms, but doesn't remove the queueing
delay itself (it's still bounded by queue capacity / consumer throughput,
just at Release's higher throughput).

## Linux profiling (not run on this machine -- macOS has no `perf`)

`perfStatAvailable()` returns `false` and `tools/run_profiled_bench.sh`
falls back to a direct run here. On Linux CI/hardware, run:

```
tools/run_profiled_bench.sh build-rel/benchmarks/bench_match_optimized
tools/run_profiled_bench.sh build-rel/benchmarks/bench_pipeline
```

which is equivalent to (and on Linux resolves to) `perf stat -d <binary>`,
reporting cache-references/cache-misses, instructions, IPC, and branch
misses for the run. Template output shape (fill in from an actual Linux run):

```
 Performance counter stats for './bench_match_optimized':

      <N>      task-clock:u
      <N>      cache-references:u
      <N>      cache-misses:u           # <PCT>% of all cache refs
      <N>      instructions:u           #  <IPC> insn per cycle
      <N>      branches:u
      <N>      branch-misses:u          # <PCT>% of all branches

      <T> seconds time elapsed
```

Not exercised here since this is a macOS dev machine -- do not read the
placeholder shape above as real numbers.

## Caveats

- Absolute Debug/Release numbers above are specific to this Apple M5 Pro
  and are not comparable across machines -- the point is the *relative*
  Debug-to-Release ratio on the same hardware, same run.
- Thread pinning env vars (`TITAN_PIN_PRODUCER_CPU`/`TITAN_PIN_CONSUMER_CPU`)
  were not exercised for a real effect on this machine, since pinning is a
  no-op on macOS -- they're wired and tested for no-crash behavior only
  (`tests/unit/test_thread_affinity.cpp`), not for a measured Linux win.
- No `perf` numbers are claimed here since none were run -- see the Linux
  section above.
