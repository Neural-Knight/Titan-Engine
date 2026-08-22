# Benchmark environment (baseline.json)

- **OS**: macOS 26.5.1 (Darwin 25.5.0, arm64)
- **CPU**: Apple M5 Pro, 18 logical CPUs
- **RAM**: 48 GiB
- **Compiler**: Apple clang 21.0.0 (Xcode toolchain), `-std=c++20`
- **Build type**: no `CMAKE_BUILD_TYPE` set by this CMakeLists (debug-equivalent, no `-O` flags). Absolute latency/throughput numbers here will not transfer to an `-O2`/`-O3` build; use them only to catch regressions run-to-run on this same unoptimized build until Module 8's harness gets a Release config wired in.
- **Google Benchmark**: v1.8.3 (FetchContent)

## Frequency pinning / noise

This machine is Apple silicon; there is no `cpufreq` and no `hw.cpufrequency` sysctl, so Google Benchmark's own frequency-detection warns and falls back:

```
Unable to determine clock rate from sysctl: hw.cpufrequency: No such file or directory
***WARNING*** Failed to set thread affinity. Estimated CPU frequency may be incorrect.
```

This does not affect the actual `steady_clock`-based timings (Google Benchmark's own note), only its reported `mhz_per_cpu` metadata field. No corrective action was taken for this baseline run; the machine was otherwise idle (load average ~1.3-1.8, background OS processes only).

- **macOS**: to reduce thermal/frequency-scaling noise before a benchmark run, sample `sudo powermetrics --samplers cpu_power -i 1000 -n 5` beforehand to confirm the CPU isn't already thermally throttled, and close other heavy apps. There is no user-facing P-state pin on Apple silicon equivalent to Linux's `cpupower frequency-set`.
- **Linux** (for future runs on Linux CI/hardware): pin governor with `sudo cpupower frequency-set --governor performance`, and prefer `perf stat -d ./bench_match` for cache/branch-miss detail; `perf record -g ./bench_match && perf report` for flamegraphs. Not exercised on this machine -- macOS has no `perf`.

## Optional Release benchmark profile (Module 15)

By default this CMakeLists still applies no `-O` flags anywhere (as above).
To build benchmark binaries and their direct library deps with `-O2 -DNDEBUG`
instead, opt in via a separate build directory:

```
cmake -S . -B build-rel -DTITAN_BENCHMARK_RELEASE=ON
cmake --build build-rel
```

`titan_tests` is unaffected either way, so `cmake -S . -B build && make test`
keeps its current debug-equivalent timing. See
`docs/benchmark-results/module-15-tuning.md` for Debug-vs-Release numbers.

## Reproducing this baseline

```
rm -rf build && make bench
```

Each `bench_*` binary prints Google Benchmark's throughput table (add `--benchmark_format=json` for machine-readable output) followed by one line like:

```
LATENCY_P50_NS=... LATENCY_P95_NS=... LATENCY_P99_NS=... LATENCY_P999_NS=...
```

from the custom `titan::run` harness (`include/titan/bench/harness.hpp`).
