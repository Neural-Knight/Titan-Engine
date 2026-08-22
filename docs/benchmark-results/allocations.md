# Memory pools and allocation counts

## Method

`benchmarks/bench_allocations.cpp` overrides global `operator new`/`delete`
(scoped to that one executable only -- `titan_tests` is a separate binary
and unaffected) to count total heap traffic. Unlike `bench_match*.cpp`
(fresh matcher per iteration, for independent latency samples), this bench
keeps **one** matcher+`OrderManager` alive across many replay passes of
`steady_limit_flow`, each pass with its order ids shifted by a disjoint
offset (so a persistent `OrderManager` doesn't reject every `Add` as a
duplicate on the 2nd+ pass). 5 warmup passes run unmeasured so the pool's
slabs finish growing before the counted window; then 10 passes (200,000 ops)
are measured. This mirrors how a real exchange matcher actually runs --
constructed once, alive for the process lifetime -- rather than the
`bench_match*` convention.

Build type: no `CMAKE_BUILD_TYPE` set (debug-equivalent), same as
`baseline.json` and `module-12-optimized.json`, for a consistent comparison.

## Results

| Matcher | Allocations (200k ops) | Deallocations | Allocs per 1M ops |
| --- | --- | --- | --- |
| ReferenceMatcher | 1,337,882 | 817,516 | 6,689,410 |
| OptimizedMatcher (pooled) | 1,265,432 | 785,168 | 6,327,160 |

**5.4% fewer allocations** after warmup. Latency (same `titan_bench` harness
as `optimized1.json`, for continuity):

| | p50 (ns) | p99 (ns) |
| --- | --- | --- |
| ReferenceMatcher | 41,537,900 | 45,050,700 |
| OptimizedMatcher | 41,255,300 | 42,011,400 |

p50 is within noise (~0.7% faster); p99 improved ~6.7%, consistent with fewer
allocation-growth spikes reducing tail latency.

## Why not closer to zero

Pooling covers **one** of four allocation sources per resting order, not all of them.

1. **Order object storage** -- now pool-backed (`FixedObjectPool<Order>` in
   `include/titan/core/memory_pool.hpp`). After warmup this is genuinely
   zero new heap traffic: a filled/canceled order's slot returns to the
   pool and is reused by *any* future order, not just ones at the same
   price (this also fixes Module 12's "unbounded per-level memory growth"
   caveat as a side effect).
2. **`Level::orders` (`vector<Order*>`)** -- still reallocates as it grows.
   Module 13 adds `reserve(8)` on a freshly-opened level to cut early
   regrowth, but a level that closes (`liveCount==0`) is still erased from
   the map and its vector's backing buffer freed; reopening that price
   later starts from an empty vector again. Not pooled.
3. **`std::map<Price, Level>` node** -- one heap node per distinct open
   price level, same frequency as `ReferenceMatcher`'s own `std::map`. Not
   pooled -- would need a custom allocator on the map itself.
4. **`std::unordered_map<OrderId, OrderRef>` node** -- one heap node per
   resting order id, same as `ReferenceMatcher`'s `orderTable`. Not pooled,
   same reason as (3).

`ReferenceMatcher`'s `std::list<Order>` push_back is exactly one of these
four costs *combined into a single list-node allocation* (node + payload
together) -- so pooling (1) alone doesn't remove list-node-equivalent cost
from (2)-(4), it only removes what was ReferenceMatcher's per-order
*payload-sized* share of that cost. Retaining empty (`liveCount==0`) levels
instead of erasing them would reduce (2)-(3) further, but `bestBid`/
`bestAsk`/`bidDepth`/`askDepth` would then need to skip zombie levels to
stay parity-correct with `ReferenceMatcher` -- judged too risky for this
module's constraint ("must remain bit-for-bit equivalent... on all parity
tests") versus the gain; left as future work.

Also note: `matchOrder`'s `vector<Trade>` return value allocates on any
crossing call, identically for both matchers -- an `OrderManager`/`IMatcher`
contract detail outside either matcher's control

## Cache misses

No `perf` on macOS (same caveat as `environment.md`/`optimized1.json`).
Allocation count and latency percentiles above are the primary metrics here.
For a future Linux run: `perf stat -d -- build/benchmarks/bench_allocations`.

## Leak check (optional, not run here)

```
cmake -S . -B build-asan -DCMAKE_CXX_FLAGS="-fsanitize=address" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"
cmake --build build-asan -j && ctest --test-dir build-asan --output-on-failure
```

`MemoryPool`'s slabs are freed by `unique_ptr` on pool/matcher destruction,
so a clean matcher teardown should show zero ASan leak reports; not verified
in this pass to keep the build count down.
